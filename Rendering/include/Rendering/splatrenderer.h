/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once
#define GL_GLEXT_PROTOTYPES
#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>
#include <QVector>
#include <QOpenGLShaderProgram>
#include <qvectornd.h>
#include <QtOpenGL/QOpenGLFunctions_4_3_Core>
#include <QOpenGLExtraFunctions>
#include "gaussiancloud.h"
 
//#include "core/program.h"
#include "core/vertexbuffer.h"

namespace rgc::radix_sort
{
    struct sorter;
}

class SplatRenderer: protected QOpenGLFunctions_4_3_Core {
public:
    SplatRenderer();
    ~SplatRenderer();

    bool Init(std::shared_ptr<GaussianCloud> gaussianCloud,
              bool isFramebufferSRGBEnabledIn, bool useRgcSortOverrideIn);

    void Sort(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat,
              const QVector4D& viewport, const QVector2D& nearFar);

    // viewport = (x, y, width, height)
    void Render(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat,
                const QVector4D& viewport, const QVector2D& nearFar);

     
    void SetupAttrib(int loc, const BinaryAttribute& attrib, int32_t count, size_t stride) {
      assert(attrib.type == BinaryAttribute::Type::Float);
      glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, (uint32_t)stride, (void*)attrib.offset);
      glEnableVertexAttribArray(loc);
    }

   public:
    uint32_t numBlocksPerWorkgroup = 1024;
protected:
    void BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud);
     QOpenGLExtraFunctions* glFuncs;
    std::shared_ptr<rgc::radix_sort::sorter> sorter;
    std::unique_ptr<QOpenGLShaderProgram> splatProg;
    std::unique_ptr<QOpenGLShaderProgram> preSortProg;
    std::unique_ptr<QOpenGLShaderProgram> histogramProg;
    std::unique_ptr<QOpenGLShaderProgram> sortProg;
   /* std::shared_ptr<Program> splatProg;
    std::shared_ptr<Program> preSortProg;
    std::shared_ptr<Program> histogramProg;
    std::shared_ptr<Program> sortProg;*/
    //std::shared_ptr<VertexArrayObject> splatVao;
    QOpenGLVertexArrayObject splatVao;
    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;
    std::vector<uint32_t> atomicCounterVec;

    std::shared_ptr<BufferObject> gaussianDataBuffer;
    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> keyBuffer2;
    std::shared_ptr<BufferObject> histogramBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> valBuffer2;
    std::shared_ptr<BufferObject> posBuffer;
    std::shared_ptr<BufferObject> atomicCounterBuffer;

    uint32_t sortCount;
    bool isFramebufferSRGBEnabled;
    bool useRgcSortOverride;

};
