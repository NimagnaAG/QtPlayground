#include "Rendering/pch.h"

#include "Rendering/GsRenderObject.h"

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

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <cmath>

namespace nimagna {

GsRenderObject::GsRenderObject(const QString& location, QRect mvp)  {
  // enableSeparateMask(false, false);
  mGsLocation = location; 
    
  mViewPort = mvp;
  m_camera.id = 0;
  m_camera.img_name = "00001";
  m_camera.width = 1959;
  m_camera.height = 1090;
  m_camera.position = QVector3D(-3.0089893469241797f, -0.11086489695181866f, -3.7527640949141428f);
  // This is a simplified direct conversion, real rotation needs proper QMatrix3x3
  m_camera.rotation =
      QMatrix3x3(new float[]{0.876134201218856f, 0.06925962026449776f, 0.47706599800804744f,
                             -0.04747421839895102f, 0.9972110940209488f, -0.057586739349882114f,
                             -0.4797239414934443f, 0.027805376500959853f, 0.8769787916452908f});
  m_camera.fy = 1164.6601287484507f;
  m_camera.fx = 1159.5880733038064f; 
  viewMatrix = QMatrix4x4(new float[]{0.47f, 0.04f, 0.88f, 0, -0.11f, 0.99f, 0.02f, 0, -0.88f,
                                      -0.11f, 0.47f, 0, 0.07f, 0.03f, 6.55f, 1});
  mProjectionMatrix = getProjectionMatrix(m_camera.fx, m_camera.fy, mViewPort.width(), mViewPort.height());
  viewMatrix.translate(m_camera.position);    // Adds the translation
  initialize();
  setupShaderProgram();

  // Done
  RenderObject::initialize();
}

void GsRenderObject::initialize() {
  initializeOpenGLFunctions();

  setupShaderProgram();
  QString fileExtension = mGsLocation.split(".").last();
  if (fileExtension == "splat") LoadSplatGs(mGsLocation);
  if (fileExtension == "vsplat")
    LoadAnimateGs(mGsLocation);
  else if (fileExtension == "ply")
    LoadGaussianCloud(mGsLocation);
  // Done
  RenderObject::initialize();
}

void GsRenderObject::LoadSplatGs(const QString& location) {
  // Clear previous data

  QFile file(location);
  if (!file.open(QFile::ReadOnly)) {
    SPDLOG_ERROR("Failed to open file: {}", location.toStdString());
    return;
  }
  QByteArray buffer = file.readAll();
  int rowLength = 32;
  vertexCount = buffer.size() / rowLength;

  // Create and configure VAO
  if (mVAO.create()) {
    mVAO.bind();
    if (!mVAO.isCreated()) {
      SPDLOG_DEBUG("Creating VertexArrayObject");
      mVAO.create();
    }

    mVBO = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    if (!mVBO.create()) {
      SPDLOG_ERROR("Failed to create VertexBufferObject");
    }
    mVBO.bind();
    const float triangleVertices[] = {-2, -2, 2, -2, 2, 2, -2, 2};  // 4 vertices, 2 components each
    mVBO.allocate(triangleVertices, sizeof(triangleVertices));
    // mShaderProgram->setUniformValue("view", viewMatrix);
    m_aPositionLoc = mShaderProgram->attributeLocation("position");
    mShaderProgram->enableAttributeArray(m_aPositionLoc);
    mShaderProgram->setAttributeBuffer(m_aPositionLoc, GL_FLOAT, 0, 2, 0);
    // mVBO.release();
    // VAO setup (replaces some of the repeated bindBuffer/vertexAttribPointer calls)
    mTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    // mTexture = std::make_unique<QOpenGLTexture>(qGlTarget());
    if (!mTexture->create()) {
      SPDLOG_ERROR("Unable to create texture");
      assert(false);
    }
    // mTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    mTexture->bind();  // bind to GL_TEXTURE_2D automatically
    mTexture->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    mTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    mShaderProgram->setUniformValue("u_texture", 0);  // Activate texture unit

    RunSort( );

  } else {
    SPDLOG_ERROR("Failed to create VAO.");
    return;
  }
}
void GsRenderObject::setupShaderProgram() {
  mShaderProgram = std::make_unique<QOpenGLShaderProgram>();
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                                        ":/resources/shaders/AnimateGS.vert")) {
    SPDLOG_ERROR("Vertex shader error! {}", mShaderProgram->log().toStdString());
  }
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/AnimateGS.frag")) {
    SPDLOG_ERROR("Fragment shader error! {}", mShaderProgram->log().toStdString());
  }
  if (!mShaderProgram->link()) {
    SPDLOG_ERROR("Shader linker error! {}", mShaderProgram->log().toStdString());
  }
  mShaderProgram->bind();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
}

void GsRenderObject::LoadAnimateGs(const QString& location) {}

QVector<quint32> GsRenderObject::generateTexture(QByteArray buffer) {
  
  const float* f_buffer = reinterpret_cast<const float*>(buffer.constData());
  const quint8* u_buffer = reinterpret_cast<const quint8*>(buffer.constData());

  int texWidth = 1024 * 2;
  int texHeight = std::ceil((2.0 * vertexCount) / texWidth);
  QVector<quint32> texdata(texWidth * texHeight * 4);  // 4 components per pixel
  quint8* texdata_c = reinterpret_cast<quint8*>(texdata.data());
  float* texdata_f = reinterpret_cast<float*>(texdata.data());

  for (int i = 0; i < vertexCount; ++i) {
    // Positions
    texdata_f[8 * i + 0] = f_buffer[8 * i + 0];
    texdata_f[8 * i + 1] = f_buffer[8 * i + 1];
    texdata_f[8 * i + 2] = f_buffer[8 * i + 2];

    // RGBA color
    texdata_c[4 * (8 * i + 7) + 0] = u_buffer[32 * i + 24];
    texdata_c[4 * (8 * i + 7) + 1] = u_buffer[32 * i + 25];
    texdata_c[4 * (8 * i + 7) + 2] = u_buffer[32 * i + 26];
    texdata_c[4 * (8 * i + 7) + 3] = u_buffer[32 * i + 27];

    // Quaternion
    float scale[3] = {f_buffer[8 * i + 3], f_buffer[8 * i + 4], f_buffer[8 * i + 5]};

    float rot[4] = {
        (u_buffer[32 * i + 28 + 0] - 128) / 128.0f, (u_buffer[32 * i + 28 + 1] - 128) / 128.0f,
        (u_buffer[32 * i + 28 + 2] - 128) / 128.0f, (u_buffer[32 * i + 28 + 3] - 128) / 128.0f};

    // Rotation matrix M = S * R
    float M[9] = {(1.0f - 2.0f * (rot[2] * rot[2] + rot[3] * rot[3])) * scale[0],
                  (2.0f * (rot[1] * rot[2] + rot[0] * rot[3])) * scale[0],
                  (2.0f * (rot[1] * rot[3] - rot[0] * rot[2])) * scale[0],

                  (2.0f * (rot[1] * rot[2] - rot[0] * rot[3])) * scale[1],
                  (1.0f - 2.0f * (rot[1] * rot[1] + rot[3] * rot[3])) * scale[1],
                  (2.0f * (rot[2] * rot[3] + rot[0] * rot[1])) * scale[1],

                  (2.0f * (rot[1] * rot[3] + rot[0] * rot[2])) * scale[2],
                  (2.0f * (rot[2] * rot[3] - rot[0] * rot[1])) * scale[2],
                  (1.0f - 2.0f * (rot[1] * rot[1] + rot[2] * rot[2])) * scale[2]};

    float sigma[6] = {
        M[0] * M[0] + M[3] * M[3] + M[6] * M[6], M[0] * M[1] + M[3] * M[4] + M[6] * M[7],
        M[0] * M[2] + M[3] * M[5] + M[6] * M[8], M[1] * M[1] + M[4] * M[4] + M[7] * M[7],
        M[1] * M[2] + M[4] * M[5] + M[7] * M[8], M[2] * M[2] + M[5] * M[5] + M[8] * M[8]};

    texdata[8 * i + 4] = packHalf2x16(4 * sigma[0], 4 * sigma[1]);
    texdata[8 * i + 5] = packHalf2x16(4 * sigma[2], 4 * sigma[3]);
    texdata[8 * i + 6] = packHalf2x16(4 * sigma[4], 4 * sigma[5]);
  }

  qDebug() << "Generated texture data with size:" << texWidth << "x" << texHeight;
  mTexture->bind();
  mTexture->setSize(texWidth, texHeight);
  mTexture->setFormat(QOpenGLTexture::RGBA32U);
  mTexture->allocateStorage();
  //const uchar* data = reinterpret_cast<const uchar*>(texdata);
  mTexture->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, texdata.data());
  // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texwidth, texheight,
  // GL_RGBA_INTEGER, GL_UNSIGNED_INT, texdata.constData());
  // glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texwidth, texheight, GL_RGBA_INTEGER, GL_UNSIGNED_INT,
  // texdata.constData()); glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, texWidth, texHeight,0,
  // GL_RGBA_INTEGER, GL_UNSIGNED_INT, texdata.data());
  mTexture->release();
  // glGenTextures(1, &mTexture);
  //  QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
  return texdata;
  // You can now use texdata as your "texture"
  // Optional: emit signalTextureReady(texdata, texWidth, texHeight);
}

void GsRenderObject::RunSort() {
 
  const float* f_buffer = reinterpret_cast<const float*>(buffer.constData());
  // Assume viewProj and lastProj are QMatrix4x4, and Positions is a QVector<float> (flat array)
  if (viewProj == QMatrix4x4()) return;  // QMatrix4x4() is the identity

  if (LastVertexCount == vertexCount) {
    QVector3D TranslationA = viewProj.column(3).toVector3D();
    QVector3D TranslationB = lastProj.column(3).toVector3D();
    float Dist = (TranslationA - TranslationB).length();
    if (Dist < 0.015f) return;
  } else {
    QVector<quint32> texture = generateTexture(buffer);
    LastVertexCount = vertexCount;
  }
  lastProj = viewProj;

  int minDepth = std::numeric_limits<int>::max();
  int maxDepth = std::numeric_limits<int>::min();
  QVector<int> SizeList(vertexCount);

  for (int i = 0; i < vertexCount; i++) {
    float x = f_buffer[8 * i + 0];
    float y = f_buffer[8 * i + 1];
    float z = f_buffer[8 * i + 2];

    float depth = viewProj(2, 0) * x + viewProj(2, 1) * y + viewProj(2, 2) * z;
    int depthInt = static_cast<int>(depth * 4096.0f);
    SizeList[i] = depthInt;
    if (depthInt > maxDepth) maxDepth = depthInt;
    if (depthInt < minDepth) minDepth = depthInt; 
  }
  int range = static_cast<int>(maxDepth - minDepth);
  float depthInv = range > 0 ? (65535.0f / range) : 1.0f;
  int ArrayMax = 65536;
  QVector<uint32_t> Counts0(65536, 0);

  for (int i = 0; i < vertexCount; i++) {
    SizeList[i] = static_cast<int>((SizeList[i] - minDepth) * depthInv);
    if (SizeList[i] >= ArrayMax) SizeList[i] = ArrayMax - 1;
    Counts0[SizeList[i]]++;
  }

  QVector<uint32_t> Starts0(ArrayMax, 0);
  for (int i = 1; i < ArrayMax; i++) Starts0[i] = Starts0[i - 1] + Counts0[i - 1];

  QVector<uint32_t> depthIndex(vertexCount, 0);
  for (int i = 0; i < vertexCount; i++) {
    depthIndex[Starts0[SizeList[i]]++] = i;
  }
  // Create and configure Index Buffer
  auto indexBuffer = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
  // auto indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
  if (indexBuffer->create()) {
    indexBuffer->bind();
    m_aIndexLoc = mShaderProgram->attributeLocation("index");
    mShaderProgram->enableAttributeArray(m_aIndexLoc);
    // Use glVertexAttribIPointer directly as QOpenGLShaderProgram::setAttributeBuffer doesn't
    // support GL_INT for attributes
    glVertexAttribIPointer(m_aIndexLoc, 1, GL_INT, 0, nullptr);
    // JS: gl.vertexAttribDivisor(a_index, 1);
    glVertexAttribDivisor(m_aIndexLoc, 1);
    indexBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
    indexBuffer->allocate(depthIndex.constData(), depthIndex.size() * 4);
  } else {
    SPDLOG_ERROR("Failed to create index buffer.");
    mVAO.release();
    return;
  }
 // return DepthIndex;
}

void GsRenderObject::draw() { ///need to fix
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }
  QMatrix4x4 inv= viewMatrix.inverted();
  viewMatrix = inv.inverted();
  QMatrix4x4 inv2 = viewMatrix.inverted();
// // float m_jumpDelta = 0;
//  inv2.translate(0.0f, -m_jumpDelta, 0.0f);
//  inv2.rotate(-0.1f * m_jumpDelta, 1.0f, 0.0f, 0.0f);
  QMatrix4x4 actualViewMatrix = inv2.inverted();

  QMatrix4x4 viewProj = mProjectionMatrix * actualViewMatrix;
  RunSort();

  mShaderProgram->bind();   
  mVAO.bind();
   mShaderProgram->setUniformValue("view", viewMatrix);
  //glUniformMatrix4fv
  mShaderProgram->setUniformValue("projection", mProjectionMatrix);
  mShaderProgram->setUniformValue("viewport", QVector2D(mViewPort.width(), mViewPort.height()));
  mShaderProgram->setUniformValue("focal", QVector2D(m_camera.fx, m_camera.fy));
  
 
  glDrawElements(GL_TRIANGLES, vertexCount, GL_UNSIGNED_SHORT, 0);
  mVAO.release();
 // glBindTexture(GL_TEXTURE_2D, 0);
  mShaderProgram->release();
}
void GsRenderObject::resizeGL(int w, int h) { // need to fix
  // JS: const downsample = ... devicePixelRatio
  // Using QWindow::devicePixelRatio()
 
  // JS: gl.uniform2fv(u_focal, new Float32Array([camera.fx, camera.fy]));
  mShaderProgram->setUniformValue("focal", QVector2D(m_camera.fx, m_camera.fy));

  // JS: projectionMatrix = getProjectionMatrix(...)
  mProjectionMatrix = getProjectionMatrix(m_camera.fx, m_camera.fy, w, h);
 
  // JS: gl.uniform2fv(u_viewport, new Float32Array([innerWidth, innerHeight]));
  mShaderProgram->setUniformValue("viewport",
                                  QVector2D(mViewPort.width(), mViewPort.height()));

  // JS: gl.uniformMatrix4fv(u_projection, false, projectionMatrix);
  mShaderProgram->setUniformValue("projection", mProjectionMatrix);
}

}  // namespace nimagna
