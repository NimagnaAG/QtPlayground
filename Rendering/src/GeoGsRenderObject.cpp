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
#include <memory>
#include <type_traits>
#include <array>
#include <vector>
#include <mdspan>

#include <cmath>

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
   if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,   ":/resources/shaders/geogs.vert")) {
     SPDLOG_ERROR("Vertex shader error! {}", mShaderProgram->log().toStdString());
   } 
   if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Geometry,   ":/resources/shaders/geogs.geom")) {
     SPDLOG_ERROR("Geom shader error! {}", mShaderProgram->log().toStdString());
   } 
   // read the fragment shader program from the resources
   if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,  ":/resources/shaders/geogs.frag")) {
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
   m_uViewportLoc = mShaderProgram->uniformLocation("uViewport");
    
   
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
   //const int positionOffsetBytes = 0;
   mShaderProgram->enableAttributeArray(0);
   mShaderProgram->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, center), 3, stride);
   //const int scaleOffsetBytes = 3 * sizeof(float);
   mShaderProgram->enableAttributeArray(1);
   mShaderProgram->setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, scale), 3, stride);
   //const int rotationOffsetBytes = scaleOffsetBytes + 3 * sizeof(float);
   mShaderProgram->enableAttributeArray(2);
   mShaderProgram->setAttributeBuffer(2, GL_FLOAT, offsetof(Vertex, rotation), 4, stride);

   //const int colorOffsetBytes = rotationOffsetBytes + scaleOffsetBytes + 4 * sizeof(float);
   mShaderProgram->enableAttributeArray(3);
   mShaderProgram->setAttributeBuffer(3, GL_FLOAT, offsetof(Vertex, color), 4, stride);
   mShaderProgram->link();
   mShaderProgram->bind();

  
   // Set up the projection matrix
   const float aspectRatio = 1.0f;
   const float nearPlane = 0.01f;
   const float farPlane = 1000.f;
    
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
  
   mProjectionMatrix.perspective(90, aspectRatio, nearPlane, farPlane);
   // mProjectionMatrix.ortho(-1.0f, 1.0f, -1.0f, 1.0f, nearPlane, farPlane);
   SPDLOG_INFO("Projection matrix setup");
   sort(mViewProjectionMatrix);  // Sort based on the view projection matrix

   
   glEnable(GL_BLEND);
   glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
   glClearColor(0, 0, 0, 0);
   isDataReady = true;
    
   //mVAO.release(); 
 
 }
 void GeoGsRenderObject::draw() {

   if (!mShaderProgram) {
     SPDLOG_ERROR("Shader program is not available.");
     return;
   }
   glClearColor(0, 0, 0, 0);
   mShaderProgram->bind();

   mShaderProgram->setUniformValue(m_uViewLoc, viewMatrix);
   mShaderProgram->setUniformValue(m_uProjLoc, mViewProjectionMatrix);
   mShaderProgram->setUniformValue(m_uFocalLoc, QVector2D(focalWidth, focalHeight));
   mShaderProgram->setUniformValue(m_uViewportLoc, QVector2D(viewportw, viewporth));

   mVAO.bind();
   glDrawElements(GL_POINTS, int(m_positions.size()), GL_UNSIGNED_INT, 0);
 
   glBindTexture(GL_TEXTURE_2D, 0);
   //mVAO.release();
   //mShaderProgram->release();
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
   m_ebo=QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
   if (!m_ebo.isCreated()) {
     SPDLOG_DEBUG("Creating ebo");
     m_ebo.create();
   } 
   m_ebo.bind();
   m_ebo.allocate(indices.data(), int(indices.size() * sizeof(uint32_t)));
 }

 void GeoGsRenderObject::resizeGL(int w, int h) {
   QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
   GLfloat tabFloat[] = {static_cast<GLfloat>(focalWidth), static_cast<GLfloat>(focalHeight)}; 
  // m_projectionMatrix = getProjectionMatrix(focalWidth, focalHeight, w, h);
   // GLfloat innerTab[] = {static_cast<GLfloat>(w), static_cast<GLfloat>(h)};
   // f->glUniform2fv(m_viewPortLoc, 1, mViewProjectionMatrix.data());
   viewportw = w;
   viewporth = h; 
   //f->glUniformMatrix4fv(m_projMatrixLoc, 1, false, m_projectionMatrix.data());
   SPDLOG_INFO("GeoGsRenderObject resizeGL done ");
 }

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
