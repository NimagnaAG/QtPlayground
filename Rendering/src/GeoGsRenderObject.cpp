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
  // setScale(0.1f);

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

  // Attention: viewport is fixed to 1080x720!
  viewportLocation = mShaderProgram->uniformLocation("uViewport");
  QSize viewportSize = QOpenGLContext::currentContext()->surface()->size();
  mShaderProgram->setUniformValue(viewportLocation,
                                  QVector2D(viewportSize.width(), viewportSize.height()));
  SPDLOG_INFO("in setupShaderProgram viewportSize.width(), viewportSize.height()",
              viewportSize.width(), viewportSize.height());
  // Note: Assuming focal length to be fixed. Is 1500x1500 a good value?????? ANSWER BY JAMES: NOT
  // GOOD VALUE, need to get focal length from camera
  focalPosition = mShaderProgram->uniformLocation("uFocal");
  auto [fx, fy] = calculateFocalLengths(fovY(), viewportSize.width(), viewportSize.height());
  QVector2D focalValue(fx, fy);
  mShaderProgram->setUniformValue(focalPosition, focalValue);
  mViewMatrix.setToIdentity(); 
  const QVector3D upVector(1, -1, 0);
  QVector3D position = QVector3D(1, 0, 0);
  mViewMatrix.lookAt(position, QVector3D(), upVector); 
  // glEnable(GL_BLEND);
}

void GeoGsRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }

  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  // sort by depth depending on the view projection matrix
  /*if (!isControlPressed) {
      mViewMatrix = viewMatrix;
  }*/

  mShaderProgram->bind();
  QSize viewportSize = QOpenGLContext::currentContext()->surface()->size();
  resizeGL(viewportSize.width(), viewportSize.height());

  // QMatrix4x4 gsprojectionMatrix = getProjectionMatrix(focalValue.x(), focalValue.y(),
  // viewportSize.width(), viewportSize.height()); sortSplatsAndUpdateIndexBufferObject(viewMatrix *
  // gsprojectionMatrix);

  // bind shader and update the view/projection matrices

  // mShaderProgram->setUniformValue(m_uViewLoc, viewMatrix);
  // mShaderProgram->setUniformValue(m_uProjLoc, gsprojectionMatrix);

  // mShaderProgram->setUniformValue(viewportLocation,  QVector2D(viewportSize.width(),
  // viewportSize.height()));

  mVAO.bind();
  mIBO.bind();
  glDrawElements(GL_POINTS, int(mSplatData.positions.size()), GL_UNSIGNED_INT, 0);
  // mVAO.release();
  // mShaderProgram->release();
}

void GeoGsRenderObject::RunSort(const QMatrix4x4& viewProj) {
  const float* f_buffer = reinterpret_cast<const float*>(data.constData());
  // Assume viewProj and lastProj are QMatrix4x4, and Positions is a QVector<float> (flat array)
  if (viewProj == QMatrix4x4() || viewProj == lastProj) {
    return;  // QMatrix4x4() is the identity
  }
  //QVector3D TranslationA = viewProj.column(3).toVector3D();
  //QVector3D TranslationB = lastProj.column(3).toVector3D();
  //float Dist = (TranslationA - TranslationB).length();

  //float dot = lastProj.column(2).z() * viewProj.column(2).z() +
  //            lastProj.column(1).z() * viewProj.column(1).z() +
  //            lastProj.column(0).z() * viewProj.column(0).z();
  //if (std::abs(dot - 1.0f) < 0.01f || Dist < 0.015f) {
  //  SPDLOG_INFO("Dist:{} < 0.015f ; dot:{} < 0.01f ", Dist, dot);
  //  return;
  //}
  float maxDepth = -std::numeric_limits<float>::infinity();
  float minDepth = std::numeric_limits<float>::infinity();
  QVector<int> SizeList(vertexCount);
  for (int i = 0; i < vertexCount; i++) {
    float depth = (viewProj(2, 0) * f_buffer[8 * i + 0] +  // viewProj[2]
                   viewProj(2, 1) * f_buffer[8 * i + 1] +  // viewProj[6]
                   viewProj(2, 2) * f_buffer[8 * i + 2]) *
                  4096.0f;  // viewProj[10]
    SizeList[i] = static_cast<int>(depth);
    if (depth > maxDepth) maxDepth = depth;
    if (depth < minDepth) minDepth = depth;
    if (i < 10) SPDLOG_INFO("depth {} {} ", i, depth);
  }

  float depthInv = (65535.0f) / (maxDepth - minDepth);
  int ArrayMax = 65536;
  QVector<uint32_t> Counts0(65536, 0);

  for (int i = 0; i < vertexCount; i++) {
    SizeList[i] = static_cast<int>((SizeList[i] - minDepth) * depthInv);
    Counts0[SizeList[i]]++;
  }

  QVector<uint32_t> Starts0(ArrayMax, 0);
  for (int i = 1; i < ArrayMax; i++) Starts0[i] = Starts0[i - 1] + Counts0[i - 1];

  QByteArray depthIndexData(vertexCount * sizeof(quint32), 0);
  quint32* depthIndex = reinterpret_cast<quint32*>(depthIndexData.data());
  // QVector<uint32_t> depthIndex(vertexCount, 0);
  for (int i = 0; i < vertexCount; i++) {
    depthIndex[Starts0[SizeList[i]]++] = i;
  }
  std::vector<uint32_t> indices(mSplatData.positions.size());
   
  lastProj = viewProj;
  if (!mIBO.isCreated()) {
    mIBO = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    if (!mIBO.create()) {
      SPDLOG_ERROR("Failed to create index buffer");
      return;
    }
    mIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
  }

  mVAO.bind();
  mIBO.bind(); 
  mIBO.allocate(depthIndexData.constData(), int(depthIndexData.size() ));
  isControlPressed = false;
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

  lastProj = viewProj;
  // update indices
  mIBO.bind();
  mIBO.setUsagePattern(QOpenGLBuffer::DynamicDraw);
  mIBO.allocate(indices.data(), int(indices.size() * sizeof(uint32_t)));
  isControlPressed = false;
  //SPDLOG_INFO("Dist:{} < 0.015f ; dot:{} < 0.01f ", Dist, dot);
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
  data = file.readAll();
  const auto rowLength = 32;  // 3 (position) + 3 (scale) + 4 (color) + 4 (quaternion)
  vertexCount = data.size() / rowLength;
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
QMatrix4x4  GeoGsRenderObject::rotate4(const QMatrix4x4& a, float rad, float x, float y, float z) {
  QMatrix4x4 rotationMatrix;
  rotationMatrix.setToIdentity();

  // Normalize the rotation axis
  QVector3D axis(x, y, z);
  axis.normalize();

  // Apply rotation around the normalized axis
  rotationMatrix.rotate(rad * 180.0f / M_PI, axis);  // Convert radians to degrees

  return a * rotationMatrix;
}

}  // namespace nimagna
