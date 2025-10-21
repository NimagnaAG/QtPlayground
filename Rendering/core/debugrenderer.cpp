/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "debugrenderer.h"
 
#define GL_GLEXT_PROTOTYPES 1
#include <memory>
#include <vector>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include "util.h"
//#include "program.h"

DebugRenderer::DebugRenderer()
{

}

bool DebugRenderer::Init()
{

    //ddProg = std::make_shared<Program>();
     ddProg  = std::make_unique<QOpenGLShaderProgram>();
    if (!ddProg->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/resources/shaders/debugdraw_vert.glsl")){
        printf("Error loading DebugRenderer shader!\n");
        return false;
    }
    if (!ddProg->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/resources/shaders/debugdraw_frag.glsl")){
        printf("Error loading DebugRenderer shader!\n");
        return false;
    }
    return true;
}

void DebugRenderer::Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
{
    linePositionVec.push_back(start);
    linePositionVec.push_back(end);
    lineColorVec.push_back(color);
    lineColorVec.push_back(color);
}

void DebugRenderer::Transform(const glm::mat4& m, float axisLen)
{
    glm::vec3 x = glm::vec3(m[0]);
    glm::vec3 y = glm::vec3(m[1]);
    glm::vec3 z = glm::vec3(m[2]);
    x = axisLen * glm::normalize(x);
    y = axisLen * glm::normalize(y);
    z = axisLen * glm::normalize(z);
    glm::vec3 p = glm::vec3(m[3]);

    Line(p, p + x, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    Line(p, p + y, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    Line(p, p + z, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void DebugRenderer::Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                           const glm::vec4& viewport, const glm::vec2& nearFar)
{
    ddProg->bind();
    glm::mat4 modelViewProjMat = projMat * glm::inverse(cameraMat);
    QMatrix4x4 qModelViewProjMat(&modelViewProjMat[0][0]);
    ddProg->setUniformValue("modelViewProjMat", qModelViewProjMat);
    //ddProg->SetAttrib("position", linePositionVec.data());
    //ddProg->SetAttrib("color", lineColorVec.data());

    int posLoc=ddProg->attributeLocation("position");
    constexpr int stride = sizeof(glm::vec3);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, linePositionVec.data());
    // Define the stride value based on the size of glm::vec3 (3 floats per vertex)

    glEnableVertexAttribArray(posLoc);

    int colorLoc = ddProg->attributeLocation("color");
    glVertexAttribPointer(colorLoc, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, lineColorVec.data());
    glEnableVertexAttribArray(colorLoc);

    glDrawArrays(GL_LINES, 0, (GLsizei)linePositionVec.size());
}

void DebugRenderer::EndFrame()
{
    linePositionVec.clear();
    lineColorVec.clear();
}
