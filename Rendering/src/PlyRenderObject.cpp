#include "Rendering/PlyRenderObject.h"
#include <QtOpenGL/QOpenGLShader> 
#include <QFile>
#include <QDebug>

namespace nimagna {

PlyRenderObject::PlyRenderObject(const QString& location) {
}
 

PlyRenderObject::~PlyRenderObject() {
    // QOpenGLShaderProgram will be deleted automatically by unique_ptr
}

void PlyRenderObject::initialize() {
    initializeOpenGLFunctions();
    setupShaderProgram();
    // Additional initialization (buffers, VAO, textures) can be added here
}

void PlyRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
    if (!mShaderProgram) return;
    mShaderProgram->bind();
    // Bind VAO, textures, set uniforms, etc.
    // glDrawArrays or glDrawElements as needed
    mShaderProgram->release();
}

void PlyRenderObject::setupShaderProgram() {
    mShaderProgram = std::make_unique<QOpenGLShaderProgram>();
    if (!mShaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, mVertexShaderFile)) {
        qWarning() << "Vertex shader failed to compile:" << mShaderProgram->log();
        return;
    }
    // Example fragment shader file path
    const QString fragmentShaderFile = ":/resources/shaders/texture.frag";
    if (!mShaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, fragmentShaderFile)) {
        qWarning() << "Fragment shader failed to compile:" << mShaderProgram->log();
        return;
    }
    if (!mShaderProgram->link()) {
        qWarning() << "Shader program failed to link:" << mShaderProgram->log();
        mShaderProgram.reset();
    }
}

} // namespace nimagna
