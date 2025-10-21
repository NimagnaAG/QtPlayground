/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "Rendering/pointrenderer.h"

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QOpenGLExtraFunctions>
#endif

#include <glm/gtc/matrix_transform.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif
#include <vector>
#include "core/image.h"
#include "core/texture.h"
#include "core/util.h"

#include "Rendering/radix_sort.hpp"

static void SetupAttrib(int loc, const BinaryAttribute& attrib, int32_t numElems, size_t stride)
{
    assert(attrib.type == BinaryAttribute::Type::Float);
  QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
  f->glVertexAttribPointer(loc, numElems, GL_FLOAT, GL_FALSE, (uint32_t)stride,
                           (void*)attrib.offset);
  f-> glEnableVertexAttribArray(loc);
}

PointRenderer::PointRenderer()
{
}

PointRenderer::~PointRenderer()
{
}

bool PointRenderer::Init(std::shared_ptr<PointCloud> pointCloud, bool isFramebufferSRGBEnabledIn)
{
    GL_ERROR_CHECK("PointRenderer::Init() begin");

    isFramebufferSRGBEnabled = isFramebufferSRGBEnabledIn;

    Image pointImg;
    if (!pointImg.Load("texture/sphere.png"))
    {
        SPDLOG_ERROR("Error loading sphere.png\n");
        return false;
    }
    pointImg.isSRGB = isFramebufferSRGBEnabled;

    Texture::Params texParams = {FilterType::LinearMipmapLinear, FilterType::Linear, WrapType::ClampToEdge, WrapType::ClampToEdge};
    pointTex = std::make_shared<Texture>(pointImg, texParams);
    pointProg= std::make_unique<QOpenGLShaderProgram>();
   // pointProg = std::make_shared<Program>();
    if (!pointProg->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/resources/shaders/point_vert.glsl")) 
    {
        SPDLOG_ERROR("Error loading point Vertex shaders!\n");
        return false;
    }
    if (!pointProg->addCacheableShaderFromSourceFile(QOpenGLShader::Geometry, ":/resources/shaders/point_geom.glsl")) 
    {
        SPDLOG_ERROR("Error loading point Vertex shaders!\n");
        return false;
    } 
    if (!pointProg->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/resources/shaders/point_frag.glsl")) 
    {
        SPDLOG_ERROR("Error loading point Vertex shaders!\n");
        return false;
    }  
    preSortProg = std::make_unique<QOpenGLShaderProgram>();
    //preSortProg = std::make_shared<Program>();
    if (!preSortProg->addCacheableShaderFromSourceFile( QOpenGLShader::Compute, ":/resources/shaders/presort_compute.glsl"))
    {
        SPDLOG_ERROR("Error loading point pre-sort compute shader!\n");
        return false;
    }

    const size_t numPoints = pointCloud->GetNumPoints();

    // build posVec
    posVec.reserve(numPoints);
    pointCloud->ForEachPosition([this](const float* pos)
    {
        posVec.emplace_back(glm::vec4(pos[0], pos[1], pos[2], pos[3]));
    });

    BuildVertexArrayObject(pointCloud);

    depthVec.resize(numPoints);
    keyBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
    valBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
    sorter = std::make_shared<rgc::radix_sort::sorter>(numPoints);

    atomicCounterVec.resize(1, 0);
    atomicCounterBuffer = std::make_shared<BufferObject>(GL_ATOMIC_COUNTER_BUFFER, atomicCounterVec, GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

    GL_ERROR_CHECK("PointRenderer::Init() end");

    return true;
}

void PointRenderer::Render(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat,
                           const QVector4D& viewport, const QVector2D& nearFar) {
    ZoneScoped;

    GL_ERROR_CHECK("PointRenderer::Render() begin");

    const size_t numPoints = posVec.size(); 
    QMatrix4x4 modelViewMat = cameraMat.inverted(); 

    const uint32_t MAX_DEPTH = std::numeric_limits<uint32_t>::max();

    {
        ZoneScopedNC("pre-sort", tracy::Color::Red4);

        preSortProg->bind();

        preSortProg->setUniformValue("modelViewProj", projMat * modelViewMat); 
        preSortProg->setUniformValue("nearFar", nearFar);
        preSortProg->setUniformValue("keyMax", MAX_DEPTH);

        QMatrix4x4 modelViewProjMat = projMat * modelViewMat;

        // reset counter back to 0
        atomicCounterVec[0] = 0;
        atomicCounterBuffer->Update(atomicCounterVec);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, atomicCounterBuffer->GetObj());

        const int LOCAL_SIZE = 256;
        QOpenGLExtraFunctions* glFuncs = QOpenGLContext::currentContext()->extraFunctions();
        glFuncs->glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1,
                                   1);  // Assuming LOCAL_SIZE threads per group
        glFuncs->glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

        GL_ERROR_CHECK("PointRenderer::Render() pre-sort");
    }

    uint32_t sortCount = 0;
    {
        ZoneScopedNC("get-count", tracy::Color::Green);

        atomicCounterBuffer->Read(atomicCounterVec);
        sortCount = atomicCounterVec[0];

        assert(sortCount <= (uint32_t)numPoints);

        GL_ERROR_CHECK("PointRenderer::Render() get-count");
    }

    {
        ZoneScopedNC("sort", tracy::Color::Red4);

        sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);

        GL_ERROR_CHECK("PointRenderer::Render() sort");
    }

    {
        ZoneScopedNC("copy-sorted", tracy::Color::DarkGreen);

        glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
        glBindBuffer(GL_COPY_WRITE_BUFFER, pointVao->GetElementBuffer()->GetObj());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, sortCount * sizeof(uint32_t));

        GL_ERROR_CHECK("PointRenderer::Render() copy-sorted");
    }

    {
        ZoneScopedNC("draw", tracy::Color::Red4);
         
        float aspectRatio = viewport.z() / viewport.w();

        pointProg->bind();
        pointProg->setUniformValue("modelViewMat", modelViewMat);
        pointProg->setUniformValue("projMat", projMat);
        pointProg->setUniformValue("pointSize", 0.02f);  // in ndc space?!?
        pointProg->setUniformValue("invAspectRatio", 1.0f / aspectRatio);

        // use texture unit 0 for colorTexture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, pointTex->texture);
        pointProg->setUniformValue("colorTex", 0);

        pointVao->Bind();
        glDrawElements(GL_POINTS, sortCount, GL_UNSIGNED_INT, nullptr);
        pointVao->Unbind();

        GL_ERROR_CHECK("PointRenderer::Render() draw");
    }
}

void PointRenderer::BuildVertexArrayObject(std::shared_ptr<PointCloud> pointCloud)
{
    pointVao = std::make_shared<VertexArrayObject>();

    const size_t numPoints = pointCloud->GetNumPoints();

    // allocate large buffer to hold interleaved vertex data
    pointDataBuffer = std::make_shared<BufferObject>(GL_ARRAY_BUFFER, pointCloud->GetRawDataPtr(),
                                                     pointCloud->GetTotalSize(), 0);

    // build element array
    indexVec.reserve(numPoints);
    assert(numPoints <= std::numeric_limits<uint32_t>::max());
    for (uint32_t i = 0; i < (uint32_t)numPoints; i++)
    {
        indexVec.push_back(i);
    }
    auto indexBuffer = std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

    pointVao->Bind();
    pointDataBuffer->Bind();

    SetupAttrib(pointProg->attributeLocation("position"), pointCloud->GetPositionAttrib(), 4,
                pointCloud->GetStride());
    SetupAttrib(pointProg->attributeLocation("color"), pointCloud->GetColorAttrib(), 4,
                pointCloud->GetStride());

    pointVao->SetElementBuffer(indexBuffer);
    pointDataBuffer->Unbind();
}
