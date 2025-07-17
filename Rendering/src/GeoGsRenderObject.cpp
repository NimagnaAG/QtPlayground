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
GeoGsRenderObject::GeoGsRenderObject(TextureTarget type) : mTextureTarget(type) {
  enableSeparateMask(false, false);
}

GeoGsRenderObject::GeoGsRenderObject(TextureTarget type, const QString& location)
    : GeoGsRenderObject(type) {
  mGsLocation = location;
  initialize();
}
void GeoGsRenderObject::initialize() {
  initializeOpenGLFunctions();

  // define model matrix
  setScale(0.01f);

  // load file
  QString fileExtension = mGsLocation.split(".").last();
  QString splatExt = QString("splat");
  if (fileExtension.contains(splatExt)) {
    LoadSplatGs(mGsLocation);
    SPDLOG_INFO("splat file extension load");
  } else if (fileExtension.contains("vsplat"))
    LoadAnimateGs(mGsLocation);
  else {
    LoadSplatGs(mGsLocation);
    SPDLOG_ERROR("file extension wrong:{} ", fileExtension.toStdString());
  }
  // Finally, build and compile the shader program
  setupShaderProgram();
  // Done
  RenderObject::initialize();
}

void GeoGsRenderObject::LoadSplatGs(const QString& location) {
  SPDLOG_INFO("in LoadSplatGs");
  SplatData splats = loadSplatFile(location);
  m_positions = splats.positions;
  m_scales = splats.scales;
  m_rotations = splats.rotations;
  m_colors = splats.colors;
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
  m_uViewLoc = mShaderProgram->uniformLocation("uView");
  m_uProjLoc = mShaderProgram->uniformLocation("uProj");
  m_uFocalLoc = mShaderProgram->uniformLocation("uFocal");
  // viewport is fixed to 1080x720!
  const auto viewportLocation = mShaderProgram->uniformLocation("uViewport");
  mShaderProgram->setUniformValue(viewportLocation, QVector2D(1080, 720));

  mVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
  if (!mVBO.create()) {
    SPDLOG_ERROR("Failed to create VertexBufferObject");
  }
  mVBO.setUsagePattern(QOpenGLBuffer::StaticDraw);

  if (!mVAO.isCreated()) {
    SPDLOG_DEBUG("Creating VertexArrayObject");
    mVAO.create();
  }
  mVAO.bind();

  int stride = sizeof(Vertex);
  // const int positionOffsetBytes = 0;
  mShaderProgram->enableAttributeArray(0);
  mShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, center), 3, stride);
  // const int scaleOffsetBytes = 3 * sizeof(float);
  mShaderProgram->enableAttributeArray(1);
  mShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, scale), 3, stride);
  // const int rotationOffsetBytes = scaleOffsetBytes + 3 * sizeof(float);
  mShaderProgram->enableAttributeArray(2);
  mShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(Vertex, rotation), 4, stride);

  // const int colorOffsetBytes = rotationOffsetBytes + scaleOffsetBytes + 4 * sizeof(float);
  mShaderProgram->enableAttributeArray(3);
  mShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(Vertex, color), 4, stride);
  mShaderProgram->link();
  mShaderProgram->bind();

  mVBO.bind();

  std::vector<Vertex> vertices;
  for (size_t i = 0; i < m_positions.size(); ++i) {
    vertices.push_back({m_positions[i], m_scales[i], m_rotations[i], m_colors[i]});
  }

  mVBO.allocate(vertices.data(), int(vertices.size() * sizeof(Vertex)));

  // Set up vertex attribute pointers
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, center));
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, scale));
  glEnableVertexAttribArray(1);

  glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)offsetof(Vertex, rotation));
  glEnableVertexAttribArray(2);

  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
  glEnableVertexAttribArray(3);

  // Create and bind Index Buffer Object (EBO) if it exists

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  glClearColor(0, 0, 0, 0);
  isDataReady = true;

  // mVAO.release();
}
void GeoGsRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }

  // sort by depth depending on the view projection matrix
  sort(viewMatrix * projectionMatrix);

  glClearColor(0, 0, 0, 0);
  mShaderProgram->bind();

  mShaderProgram->setUniformValue(m_uViewLoc, viewMatrix);
  mShaderProgram->setUniformValue(m_uProjLoc, projectionMatrix);
  mShaderProgram->setUniformValue(m_uFocalLoc, QVector2D(focalWidth, focalHeight));

  mVAO.bind();
  glDrawElements(GL_POINTS, int(m_positions.size()), GL_UNSIGNED_INT, 0);

  glBindTexture(GL_TEXTURE_2D, 0);
  // mVAO.release();
  // mShaderProgram->release();
}

void GeoGsRenderObject::sort(const QMatrix4x4& viewProj) {
  std::vector<uint32_t> indices(m_positions.size());
  std::iota(indices.begin(), indices.end(), 0);

  if (!viewProj.isIdentity()) {
    std::vector<std::pair<float, uint32_t>> depthIndex;
    for (size_t i = 0; i < m_positions.size(); ++i) {
      QVector4D pos4(m_positions[i], 1.0f);
      QVector4D cam = viewProj * pos4;
      depthIndex.emplace_back(cam.z(), uint32_t(i));
    }
    std::sort(depthIndex.begin(), depthIndex.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = depthIndex[i].second;
  }
  m_ebo = QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
  if (!m_ebo.isCreated()) {
    SPDLOG_DEBUG("Creating ebo");
    m_ebo.create();
  }
  m_ebo.bind();
  m_ebo.allocate(indices.data(), int(indices.size() * sizeof(uint32_t)));
}

// void GeoGsRenderObject::resizeGL(int w, int h) {
//   QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
//   GLfloat tabFloat[] = {static_cast<GLfloat>(focalWidth), static_cast<GLfloat>(focalHeight)};
//   // m_projectionMatrix = getProjectionMatrix(focalWidth, focalHeight, w, h);
//   // GLfloat innerTab[] = {static_cast<GLfloat>(w), static_cast<GLfloat>(h)};
//   // f->glUniform2fv(m_viewPortLoc, 1, mViewProjectionMatrix.data());
//   viewportw = w;
//   viewporth = h;
//   // f->glUniformMatrix4fv(m_projMatrixLoc, 1, false, m_projectionMatrix.data());
//   SPDLOG_INFO("GeoGsRenderObject resizeGL done ");
// }

const std::map<GeoGsRenderObject::SourcePixelFormat, QImage::Format>
    GeoGsRenderObject::kSourcePixelFormatToQImageFormatMap = {
        {GeoGsRenderObject::SourcePixelFormat::RGB, QImage::Format::Format_RGB888},
        {GeoGsRenderObject::SourcePixelFormat::RGBA, QImage::Format::Format_RGBA8888_Premultiplied},
        {GeoGsRenderObject::SourcePixelFormat::BGRA,
         QImage::Format::Format_RGBA8888_Premultiplied}};

GeoGsRenderObject::~GeoGsRenderObject() {
  mVAO.destroy();
  mVBO.destroy();
  mIBO.destroy();
  mTexture.reset();
  mMaskTexture.reset();
  mShaderProgram.reset();

  mTransformMatrix.setToIdentity();
}

bool GeoGsRenderObject::hasSeparateMask() const {
  return mSeparateMaskTextureEnabled;
}

void GeoGsRenderObject::enableSeparateMask(bool separateMaskEnabled, bool blurEnabled) {
  SPDLOG_DEBUG("Texture render object: Separate mask {} / Blur {}",
               separateMaskEnabled ? "enabled" : "disabled", blurEnabled ? "enabled" : "disabled");
  if (mSeparateMaskTextureEnabled != separateMaskEnabled) {
    mSeparateMaskTextureEnabled = separateMaskEnabled;
    if (mSeparateMaskTextureEnabled) {
      mCameraMaskBlurring = blurEnabled;
    }
    initialize();
  }
}

nimagna::GeoGsRenderObject::SplatData GeoGsRenderObject::loadSplatFile(const QString& filePath) {
  SplatData result;

  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    // Handle error as appropriate
    return result;
  }
  QByteArray data = file.readAll();
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

void GeoGsRenderObject::useExternalTexture(bool useExternal) {
  SPDLOG_DEBUG("Using external texture for rendering");
  mUseExternalTexture = useExternal;
}

const QSize& GeoGsRenderObject::textureSourceSize() const {
  return mTextureSourceSize;
}

const QSize& GeoGsRenderObject::maskSourceSize() const {
  return mMaskSourceSize;
}

const QSize& GeoGsRenderObject::textureSize() const {
  return mTextureSize;
}

const QSize& GeoGsRenderObject::maskSize() const {
  return mMaskSize;
}

GeoGsRenderObject::SourcePixelFormat GeoGsRenderObject::sourcePixelFormat() const {
  return mSourcePixelFormat;
}

const QOpenGLTexture::Target GeoGsRenderObject::qGlTarget() const {
  return qGlTarget(mTextureTarget);
}

QOpenGLTexture::Target GeoGsRenderObject::qGlTarget(TextureTarget target) {
  return target == TextureTarget::Target2D ? QOpenGLTexture::Target2D
                                           : QOpenGLTexture::TargetRectangle;
}

const GLint GeoGsRenderObject::glTarget() const {
  return glTarget(mTextureTarget);
}

GLint GeoGsRenderObject::glTarget(TextureTarget target) {
  return target == GeoGsRenderObject::TextureTarget::Target2D ? GL_TEXTURE_2D
                                                              : GL_TEXTURE_RECTANGLE;
}

const QOpenGLTexture::PixelFormat GeoGsRenderObject::qGlSourceFormat() const {
  return qGlSourceFormat(mSourcePixelFormat);
}

QOpenGLTexture::PixelFormat GeoGsRenderObject::qGlSourceFormat(SourcePixelFormat format) {
  switch (format) {
    case SourcePixelFormat::RGB:
      return QOpenGLTexture::PixelFormat::RGB;
    case SourcePixelFormat::RGBA:
    case SourcePixelFormat::BGRA:
      // Note: BGRA is interpreted as RGBA and transformed in the fragment shader!
      return QOpenGLTexture::PixelFormat::RGBA;
    default:
      SPDLOG_ERROR("Unknown pixel format!");
      assert(false);
      break;
  }
  return QOpenGLTexture::PixelFormat::RGB;
}

GLint GeoGsRenderObject::glSourceFormat(SourcePixelFormat format) {
  switch (format) {
    case SourcePixelFormat::RGB:
      return GL_RGB;
    case SourcePixelFormat::RGBA:
    case SourcePixelFormat::BGRA:
      // Note: BGRA is interpreted as RGBA and transformed in the fragment shader!
      return GL_RGBA;
    default:
      SPDLOG_ERROR("Unknown pixel format!");
      assert(false);
      break;
  }
  return GL_RGB;
}

const GLint GeoGsRenderObject::glSourceFormat() const {
  return glSourceFormat(mSourcePixelFormat);
}

QImage::Format GeoGsRenderObject::qImageFormatFromSourcePixelFormat(SourcePixelFormat format) {
  return kSourcePixelFormatToQImageFormatMap.at(format);
}

}  // namespace nimagna
