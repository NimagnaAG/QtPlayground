#include "Rendering/pch.h"

#include "Rendering/GeoGsRenderObject.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QtCore/QMutexLocker>
#include <QtCore/QRandomGenerator>
#include <QtCore/QThread>
#include <QtGlobal>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLPixelTransferOptions>
#include <array>
#include <cmath>
#include <mdspan>
#include <memory>
#include <type_traits>
#include <vector>

namespace nimagna {

GeoGsRenderObject::GeoGsRenderObject(const QString& location) : mGsLocation(location) {
  initialize();
}
void GeoGsRenderObject::initialize() {
  initializeOpenGLFunctions();

  // set initial scale to 0.1f
  setScale(0.1f);

  // load file
  QString fileExtension = mGsLocation.split(".").last();
  QString splatExt = QString("splat");
  if (fileExtension.contains(splatExt)) {
    LoadSplatGs(mGsLocation);
    SPDLOG_INFO("splat file extension load");
  } else if (fileExtension.contains("vsplat")) {
    LoadAnimateGs(mGsLocation);
  } else {
    SPDLOG_ERROR("file extension wrong:{} ", fileExtension.toStdString());
    LoadSplatGs(mGsLocation);
  }
  // create vertex buffer object
  mVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  if (!mVBO.create()) {
    SPDLOG_ERROR("Failed to create VertexBufferObject");
  }
  mVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);

  // the data per vertex
  struct VertexData {
    QVector3D center;
    QVector3D scale;
    QVector4D rotation;
    QVector4D color;
  };
  std::vector<VertexData> vertices;
  for (size_t i = 0; i < mSplatData.positions.size(); ++i) {
    vertices.push_back({mSplatData.positions[i], mSplatData.scales[i], mSplatData.rotations[i],
                        mSplatData.colors[i]});
  }
  // allocate the vertex buffer object with the vertex data
  mVBO.bind();
  mVBO.allocate(vertices.data(), int(vertices.size() * sizeof(VertexData)));

  // create and bind vertex array object
  if (!mVAO.isCreated()) {
    SPDLOG_DEBUG("Creating VertexArrayObject");
    mVAO.create();
  }
  mVAO.bind();

  // Create a new buffer for the indexes
  mIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);  // Mind: use 'IndexBuffer' here
  if (!mIBO.create()) {
    SPDLOG_ERROR("Failed to create IndexBufferObject");
  }
  mIBO.setUsagePattern(QOpenGLBuffer::StaticDraw);

  // Build and compile the shader program, get the variable locations
  setupShaderProgram();

  int stride = sizeof(VertexData);
  mShaderProgram->enableAttributeArray(0);
  mShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(VertexData, center), 3, stride);
  mShaderProgram->enableAttributeArray(1);
  mShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(VertexData, scale), 3, stride);
  mShaderProgram->enableAttributeArray(2);
  mShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(VertexData, rotation), 4, stride);
  mShaderProgram->enableAttributeArray(3);
  mShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(VertexData, color), 4, stride);

  // Note: I think the lines below do the same as the above lines...

  // Set up vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                        (void*)offsetof(VertexData, center));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                        (void*)offsetof(VertexData, scale));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                        (void*)offsetof(VertexData, rotation));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                        (void*)offsetof(VertexData, color));
  glEnableVertexAttribArray(3);

  // Done
  RenderObject::initialize();
}

void GeoGsRenderObject::LoadSplatGs(const QString& location) {
  SPDLOG_INFO("in LoadSplatGs");
  mSplatData = loadSplatFile(location);
}

void GeoGsRenderObject::setupShaderProgram() {
  // create shader program
  mShaderProgram = std::make_unique<QOpenGLShaderProgram>();
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                                        ":/resources/shaders/geogs.vert")) {
    SPDLOG_ERROR("Vertex shader error! {}", mShaderProgram->log().toStdString());
  }
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Geometry,
                                                        ":/resources/shaders/geogs.geom")) {
    SPDLOG_ERROR("Geom shader error! {}", mShaderProgram->log().toStdString());
  }
  // read the fragment shader program from the resources
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/geogs.frag")) {
    SPDLOG_ERROR("Fragment shader error! {}", mShaderProgram->log().toStdString());
  }
  if (!mShaderProgram->link()) {
    SPDLOG_ERROR("Shader linker error! {}", mShaderProgram->log().toStdString());
  }
  // and bind
  if (!mShaderProgram->bind()) {
    SPDLOG_ERROR("Failed to bind shader program! {}", mShaderProgram->log().toStdString());
  }

  // get variable locations
  m_uViewLoc = mShaderProgram->uniformLocation("uView");
  m_uProjLoc = mShaderProgram->uniformLocation("uProj");

  // Note: Assuming focal length to be fixed. Is 1500x1500 a good value??????
  auto focalPosition = mShaderProgram->uniformLocation("uFocal");
  mShaderProgram->setUniformValue(focalPosition, QVector2D(1500, 1500));
  // Attention: viewport is fixed to 1080x720!
  const auto viewportLocation = mShaderProgram->uniformLocation("uViewport");
  mShaderProgram->setUniformValue(viewportLocation, QVector2D(1080, 720));
}

void GeoGsRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }

  // sort by depth depending on the view projection matrix
  sortSplatsAndUpdateIndexBufferObject(viewMatrix * projectionMatrix);

  // bind shader and update the view/projection matrices
  mShaderProgram->bind();
  mShaderProgram->setUniformValue(m_uViewLoc, viewMatrix);
  mShaderProgram->setUniformValue(m_uProjLoc, projectionMatrix);

  mVAO.bind();
  glDrawElements(GL_POINTS, int(mSplatData.positions.size()), GL_UNSIGNED_INT, 0);
  mVAO.release();
  mShaderProgram->release();
}

void GeoGsRenderObject::sortSplatsAndUpdateIndexBufferObject(const QMatrix4x4& viewProj) {
  // temporary index array for sorting
  std::vector<uint32_t> indices(mSplatData.positions.size());
  std::iota(indices.begin(), indices.end(), 0);

  if (!viewProj.isIdentity()) {
    std::vector<std::pair<float, uint32_t>> depthIndex;
    for (size_t i = 0; i < mSplatData.positions.size(); ++i) {
      QVector4D pos4(mSplatData.positions[i], 1.0f);
      QVector4D cam = viewProj * pos4;
      depthIndex.emplace_back(cam.z(), uint32_t(i));
    }
    std::sort(depthIndex.begin(), depthIndex.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = depthIndex[i].second;
  }
  // update indices
  mIBO.bind();
  mIBO.allocate(indices.data(), int(indices.size() * sizeof(uint32_t)));
}

GeoGsRenderObject::~GeoGsRenderObject() {
  mVAO.destroy();
  mVBO.destroy();
  mIBO.destroy();
  mShaderProgram.reset();
}

nimagna::GeoGsRenderObject::SplatData GeoGsRenderObject::loadSplatFile(const QString& filePath) {
  SplatData result;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    // Handle error as appropriate
    return result;
  }
  QByteArray data = file.readAll();
  const auto rowLength = 32;  // 3 (position) + 3 (scale) + 4 (color) + 4 (quaternion)
  const auto vertexCount = data.size() / rowLength;
  const uint8_t* raw = reinterpret_cast<const uint8_t*>(data.constData());

  for (int i = 0; i < vertexCount; ++i) {
    int offset = i * rowLength;

    // Position (3 floats)
    float px, py, pz;
    std::memcpy(&px, raw + offset, 4);
    std::memcpy(&py, raw + offset + 4, 4);
    std::memcpy(&pz, raw + offset + 8, 4);
    result.positions.emplace_back(px, py, pz);

    // Scale (3 floats)
    float sx, sy, sz;
    std::memcpy(&sx, raw + offset + 12, 4);
    std::memcpy(&sy, raw + offset + 16, 4);
    std::memcpy(&sz, raw + offset + 20, 4);
    result.scales.emplace_back(sx, sy, sz);

    // Color (4 bytes, normalized to [0,1])
    float r = raw[offset + 24] / 255.0f;
    float g = raw[offset + 25] / 255.0f;
    float b = raw[offset + 26] / 255.0f;
    float a = raw[offset + 27] / 255.0f;
    result.colors.emplace_back(r, g, b, a);

    // Quaternion (4 bytes, mapped to [-1,1])
    float qx = (raw[offset + 28] - 128) / 128.0f;
    float qy = (raw[offset + 29] - 128) / 128.0f;
    float qz = (raw[offset + 30] - 128) / 128.0f;
    float qw = (raw[offset + 31] - 128) / 128.0f;
    result.rotations.emplace_back(qx, qy, qz, qw);
  }
  return result;
}

}  // namespace nimagna
