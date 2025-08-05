#include "Rendering/PlyRenderObject.h"
#include <QtOpenGL/QOpenGLShader> 
#include <QFile>
#include <QDebug>
#include <filesystem>
#include <iostream>

// Include the necessary headers
#include "core/log.h"

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
/*
// searches for file named configFilename, dir that contains plyFilename, it's parent and
// grandparent dirs.
std::string PlyRenderObject::FindConfigFile(const QString& plyFilename,
                                                   const QString& configFilename) {
  std::filesystem::path plyPath(plyFilename.toStdString());

  if (!std::filesystem::exists(plyPath) || !std::filesystem::is_regular_file(plyPath)) {
    Log::E("PLY file does not exist or is not a file: \"%s\"", plyFilename.toStdString().c_str());
    return "";
  }

  std::filesystem::path directory = plyPath.parent_path();

  for (int i = 0; i < 3; ++i)  // Check current, parent, and grandparent directories
  {
    std::filesystem::path configPath = directory / configFilename.toStdString();
    if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
      return configPath.string();
    }
    if (directory.has_parent_path()) {
      directory = directory.parent_path();
    } else {
      break;
    }
  }

  return "";
}

std::string PlyRenderObject::GetFilenameWithoutExtension(const QString& filepath) {
    std::filesystem::path pathObj(filepath.toStdString());

    // Check if the path has a stem (the part of the path before the extension)
    if (pathObj.has_stem()) {
        return pathObj.stem().string();
    }

    // If there is no stem, return an empty string
    return "";
}

std::string PlyRenderObject::MakeVrConfigFilename(const QString& plyFilename) {
    std::filesystem::path plyPath(plyFilename.toStdString());
    std::filesystem::path directory = plyPath.parent_path();
    std::string plyNoExt = GetFilenameWithoutExtension(plyFilename) + "_vr.json";
    std::filesystem::path configPath = directory / plyNoExt;
    return configPath.string();
}
 */


} // namespace nimagna
