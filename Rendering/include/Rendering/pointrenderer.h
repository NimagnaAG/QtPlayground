/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <stdint.h>
#include <vector>

//#include "core/program.h"
#include "core/texture.h"
#include "core/vertexbuffer.h"
#include <Qvector>
#include "pointcloud.h"
#include "Renderer.h"
#include "Rendering/Rendering.h" 
namespace rgc::radix_sort
{
    struct sorter;
}

class PointRenderer : protected QOpenGLFunctions_4_0_Core {
public:
    PointRenderer();
    ~PointRenderer();

    bool Init(std::shared_ptr<PointCloud> pointCloud, bool isFramebufferSRGBEnabledIn);

    // viewport = (x, y, width, height)
    //void Render(const glm::mat4& cameraMat, const glm::mat4& projMat, const glm::vec4& viewport, const glm::vec2& nearFar);
    void Render(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat, const QVector4D& viewport, const QVector2D& nearFar);

   protected:
    void BuildVertexArrayObject(std::shared_ptr<PointCloud> pointCloud);

    std::shared_ptr<Texture> pointTex;
    std::unique_ptr<QOpenGLShaderProgram> pointProg;
    std::unique_ptr<QOpenGLShaderProgram> preSortProg;
    //std::shared_ptr<Program> pointProg;
   // std::shared_ptr<Program> preSortProg;
    std::shared_ptr<VertexArrayObject> pointVao;

    std::shared_ptr<BufferObject> pointDataBuffer;

    std::vector<uint32_t> indexVec;
    std::vector<uint32_t> depthVec;
    std::vector<glm::vec4> posVec;
    std::vector<uint32_t> atomicCounterVec;

    std::shared_ptr<BufferObject> keyBuffer;
    std::shared_ptr<BufferObject> valBuffer;
    std::shared_ptr<BufferObject> posBuffer;
    std::shared_ptr<BufferObject> atomicCounterBuffer;

    std::shared_ptr<rgc::radix_sort::sorter> sorter;
    bool isFramebufferSRGBEnabled;
};
