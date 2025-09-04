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

GeoGsRenderObject::~GeoGsRenderObject() {
  mVAO.destroy();
  mVBO.destroy();
  mIBO.destroy();
  mShaderProgram.reset();
}

void GeoGsRenderObject::initialize() {
  initializeOpenGLFunctions();

  // load file
  QString fileExtension = mGsLocation.split(".").last();
  if (fileExtension.contains("splat")) {
    loadSplatGs(mGsLocation);
    SPDLOG_INFO("splat file extension load");
  } else if (fileExtension.contains("vsplat")) {
    loadAnimateGs(mGsLocation);
  } else {
    SPDLOG_ERROR("file extension wrong:{} ", fileExtension.toStdString());
    loadSplatGs(mGsLocation);
  }
  // Build and compile the shader program, get the variable locations
  setupShaderProgram();
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
  mIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);

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

void GeoGsRenderObject::loadSplatGs(const QString& location) {
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
  mViewMatrixShaderLocation = mShaderProgram->uniformLocation("uView");
  mProjectionMatrixShaderLocation = mShaderProgram->uniformLocation("uProj");
  mFocalShaderLocation = mShaderProgram->uniformLocation("uFocal");
  // since viewport size does not change, we can set it once here
  mViewportShaderLocation = mShaderProgram->uniformLocation("uViewport");
  const auto viewportSize = QOpenGLContext::currentContext()->screen()->size();
  mShaderProgram->setUniformValue(mViewportShaderLocation,
                                  QVector2D(viewportSize.width(), viewportSize.height()));
}

void GeoGsRenderObject::draw(const std::shared_ptr<RenderData> renderData) {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }
  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);

  mShaderProgram->bind();
  updateIfViewProjectionChanged(renderData);

  mVAO.bind();
  mIBO.bind();
  glDrawElements(GL_POINTS, int(mSplatData.positions.size()), GL_UNSIGNED_INT, 0);

  mVAO.release();
  mShaderProgram->release();
}

nimagna::GeoGsRenderObject::SplatData GeoGsRenderObject::loadSplatFile(const QString& filePath) {
  SplatData result;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    // Handle error as appropriate
    return result;
  }
  auto data = file.readAll();
  const auto rowLength = 32;  // 3 (position) + 3 (scale) + 4 (color) + 4 (quaternion)
  auto vertexCount = data.size() / rowLength;
  const auto* raw = reinterpret_cast<const uint8_t*>(data.constData());

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

void GeoGsRenderObject::updateIfViewProjectionChanged(
    const std::shared_ptr<RenderData> renderData) {
  // get the projection and view matrices
  const auto projectionMatrix = renderData->projectionMatrix();
  const auto viewMatrix = renderData->viewMatrix();
  const auto& mvp = projectionMatrix * viewMatrix * modelMatrix();
  if (mvp == mLastMVP) {
    // do not update if the view-projection matrix has not changed
    return;
  }
  mLastMVP = mvp;

  // calculate focal lengths based on the vertical field of view and viewport size
  auto [fx, fy] = renderData->calculateFocalLengths();
  // set the focal lengths, projection and view matrix in the shader
  mShaderProgram->setUniformValue(mFocalShaderLocation, QVector2D{fx, fy});
  mShaderProgram->setUniformValue(mProjectionMatrixShaderLocation, projectionMatrix);
  mShaderProgram->setUniformValue(mViewMatrixShaderLocation, viewMatrix * modelMatrix());

  // sort splats based on the new model-view-projection matrix and update the index buffer
  sortSplatsAndUpdateIndexBufferObject(mvp);
}

void GeoGsRenderObject::sortSplatsAndUpdateIndexBufferObject(const QMatrix4x4& viewProj) {
  // temporary index array for sorting
  /* QVector3D TranslationA = viewProj.column(3).toVector3D();
   QVector3D TranslationB = lastProj.column(3).toVector3D();
   float Dist = (TranslationA - TranslationB).length();

   float dot = lastProj.column(2).z() * viewProj.column(2).z() +
               lastProj.column(1).z() * viewProj.column(1).z() +
               lastProj.column(0).z() * viewProj.column(0).z();
   if (std::abs(dot - 1.0f) < 0.01f || Dist < 0.01f) {
     return;
   }*/
  std::vector<uint32_t> indices(mSplatData.positions.size());
  std::iota(indices.begin(), indices.end(), 0);
  /* // Original sorting method using std::sort, slow for large datasets
    std::vector<std::pair<float, uint32_t>> depthIndex;
    for (size_t i = 0; i < mSplatData.positions.size(); ++i) {
      QVector4D pos4(mSplatData.positions[i], 1.0f);
      QVector4D cam = viewProj * pos4;
      depthIndex.emplace_back(cam.z(), uint32_t(i));
    }
    std::sort(depthIndex.begin(), depthIndex.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = depthIndex[i].second;
  */

  // Use a radix sort for faster sorting of depth indices
  std::vector<float> depths(mSplatData.positions.size());
  for (size_t i = 0; i < mSplatData.positions.size(); ++i) {
    QVector4D pos4(mSplatData.positions[i], 1.0f);
    QVector4D cam = viewProj * pos4;
    depths[i] = cam.z();
  }

  // Radix sort for 32-bit floats (reinterpret as uint32_t for sorting)
  std::vector<uint32_t> temp_indices(indices.size());
  std::vector<uint32_t> temp_buffer(indices.size());
  constexpr int BITS = 8;
  constexpr int BUCKETS = 1 << BITS;
  constexpr int PASSES = (32 + BITS - 1) / BITS;

  // Convert float to sortable uint32_t (handle sign bit)
  auto floatFlip = [](float f) -> uint32_t {
    uint32_t x = *reinterpret_cast<uint32_t*>(&f);
    return x ^ ((x >> 31) ? 0xFFFFFFFF : 0x80000000);
  };

  for (int pass = 0; pass < PASSES; ++pass) {
    int shift = pass * BITS;
    std::array<size_t, BUCKETS> count = {0};
    for (size_t i = 0; i < indices.size(); ++i) {
      uint32_t key = (floatFlip(depths[indices[i]]) >> shift) & (BUCKETS - 1);
      ++count[key];
    }
    std::array<size_t, BUCKETS> offset = {0};
    for (int i = 1; i < BUCKETS; ++i) {
      offset[i] = offset[i - 1] + count[i - 1];
    }
    for (size_t i = 0; i < indices.size(); ++i) {
      uint32_t key = (floatFlip(depths[indices[i]]) >> shift) & (BUCKETS - 1);
      temp_indices[offset[key]++] = indices[i];
    }
    indices.swap(temp_indices);
  }

  // update indices
  mIBO.bind();
  mIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
  mIBO.allocate(indices.data(), int(indices.size() * sizeof(uint32_t)));
  // SPDLOG_INFO("Dist:{} < 0.015f ; dot:{} < 0.01f ", Dist, dot);
}

}  // namespace nimagna
