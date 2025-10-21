#include "Rendering/PlyRenderObject.h"

#include <QDebug>
#include <QFile>
#include <QtOpenGL/QOpenGLShader>
#include <filesystem>
#include <iostream>

namespace nimagna {

PlyRenderObject::PlyRenderObject(const QString& location) : mGsLocation(location) {
  initialize();
}

PlyRenderObject::~PlyRenderObject() {
  // QOpenGLShaderProgram will be deleted automatically by unique_ptr
}

void PlyRenderObject::initialize() {
  initializeOpenGLFunctions();
  // setupShaderProgram();
  //  Additional initialization (buffers, VAO, textures) can be added here

//  glDisable(GL_FRAMEBUFFER_SRGB);
  bool isFramebufferSRGBEnabled = false;

  // gaussianCloud = LoadGaussianCloud(mGsLocation, opt);

  GaussianCloud::Options options = {0};

  options.importFullSH = true;
  options.exportFullSH = true;
  gaussianCloud = std::make_shared<GaussianCloud>(options);
  if (!gaussianCloud->ImportPly(mGsLocation.toStdString())) {
    SPDLOG_ERROR("Error loading GaussianCloud!\n");
    return;
  }
  splatRenderer = std::make_shared<SplatRenderer>();

  bool useRgcSortOverride = false;
  if (!splatRenderer->Init(gaussianCloud, isFramebufferSRGBEnabled, useRgcSortOverride)) {
    SPDLOG_ERROR("Error initializing splat renderer!\n");
    return;
  }
  desktopProgram = std::make_unique<QOpenGLShaderProgram>();
  if (!desktopProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                                        ":/resources/shaders/desktop_vert.glsl")) {
    SPDLOG_ERROR("Error loading Vertex shader!\n");
    return;
  }
  if (!desktopProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/desktop_frag.glsl")) {
    SPDLOG_ERROR("Error loading Fragment shader!\n");
    return;
  }
}

void PlyRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  
  Clear();
  QSize viewportSize = QOpenGLContext::currentContext()->screen()->size();
 
  QVector4D viewport(0.0f, 0.0f, (float)viewportSize.width(), (float)viewportSize.height());
  QVector2D nearFar(znear, zfar);
  gsprojectionMatrix.perspective(fovY(), (float)viewportSize.width() / (float)viewportSize.height(),  znear, zfar);
  // Convert gsprojectionMatrix (assumed QMatrix4x4) to glm::mat4 
 /* if ( pointRenderer) {
    pointRenderer->Render(cameraMat, projMat, viewport, nearFar);
  } else {*/
        splatRenderer->Sort(viewMatrix, gsprojectionMatrix, viewport, nearFar);
        splatRenderer->Render(viewMatrix, gsprojectionMatrix, viewport, nearFar);
  //}
        /*
  if (opt.frameBuffer != Options::FrameBuffer::Default && fbo) {
    // render fbo colorTexture as a full screen quad to the default fbo
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    Clear( );

     int width = viewportSize.width();
    int height = viewportSize.height();

    glViewport(0, 0, width, height);
    glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glm::mat4 projMat = glm::ortho(0.0f, (float)width, 0.0f, (float)height, -10.0f, 10.0f);

    if (colorTexture > 0) {
      desktopProgram->Bind();
      desktopProgram->SetUniform("modelViewProjMat", projMat);
      desktopProgram->SetUniform("color", glm::vec4(1.0f));

      // use texture unit 0 for colorTexture
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, colorTexture);
      desktopProgram->SetUniform("colorTexture", 0);

      glm::vec2 xyLowerLeft(0.0f, 0.0f);
      glm::vec2 xyUpperRight((float)width, (float)height);

      glm::vec2 uvLowerLeft(0.0f, 0.0f);
      glm::vec2 uvUpperRight(1.0f, 1.0f);

      float depth = -9.0f;
      glm::vec3 positions[] = {
          glm::vec3(xyLowerLeft, depth), glm::vec3(xyUpperRight.x, xyLowerLeft.y, depth),
          glm::vec3(xyUpperRight, depth), glm::vec3(xyLowerLeft.x, xyUpperRight.y, depth)};
      desktopProgram->SetAttrib("position", positions);

      glm::vec2 uvs[] = {uvLowerLeft, glm::vec2(uvUpperRight.x, uvLowerLeft.y), uvUpperRight,
                         glm::vec2(uvLowerLeft.x, uvUpperRight.y)};
      desktopProgram->SetAttrib("uv", uvs);

      const size_t NUM_INDICES = 6;
      uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
      glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
    }
  } */ 
}

void PlyRenderObject::setupShaderProgram() {}
// searches for file named configFilename, dir that contains plyFilename, it's parent and
// grandparent dirs.
QString PlyRenderObject::FindConfigFile(const QString& plyFilename, const QString& configFilename) {
  std::filesystem::path plyPath(plyFilename.toStdString());
  if (!std::filesystem::exists(plyPath) || !std::filesystem::is_regular_file(plyPath)) {
    qWarning() << "PLY file does not exist or is not a file: " << plyFilename.toStdString().c_str();
    return "";
  }
  std::filesystem::path directory = plyPath.parent_path();
  for (int i = 0; i < 3; ++i)  // Check current, parent, and grandparent directories
  {
    std::filesystem::path configPath = directory / configFilename.toStdString();
    if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
      return QString::fromStdString(configPath.string());
    }
    if (directory.has_parent_path()) {
      directory = directory.parent_path();
    } else {
      break;
    }
  }
  return "";
}

QString PlyRenderObject::GetFilenameWithoutExtension(const QString& filepath) {
  std::filesystem::path pathObj(filepath.toStdString());
  // Check if the path has a stem (the part of the path before the extension)
  if (pathObj.has_stem()) {
    return QString::fromStdString(pathObj.stem().string());
  }
  // If there is no stem, return an empty string
  return "";
}

void PlyRenderObject::Clear() {
  //// pre-multiplied alpha blending
  // glEnable(GL_BLEND);
  ////glBlendEquation(GL_FUNC_ADD);
  // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
  // glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //// NOTE: if depth buffer has less then 24 bits, it can mess up splat rendering.
  // glEnable(GL_DEPTH_TEST);
}

}  // namespace nimagna
