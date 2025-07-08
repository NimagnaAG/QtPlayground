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
#include <array>
#include <vector>
#include <mdspan>

#include <cmath>

namespace nimagna {

GsRenderObject::GsRenderObject(const QString& location) 
  : m_texture(QOpenGLTexture::Target2D),  // Initialize m_texture with a valid constructor
    viewMatrix(nullptr, std::extents<std::size_t, 4, 4>()) {  // Initialize viewMatrix with nullptr first
  // Properly initialize std::mdspan
  static float initialMatrix[16] = {1.f, 0.f,  0.f, 0.f, 
                                    0.f, 1.f, 0.f, 0.f,
                                    0.f, 0.f, 1.f, 0.0f, 
                                    0.7f,  0.0f, 0.55f, 1.0f};

  viewMatrix = std::mdspan<float, std::extents<std::size_t, 4, 4>>(initialMatrix);
  mGsLocation = location;  
  initialize(); 
}

void GsRenderObject::initialize() {
  initializeOpenGLFunctions();
   
  // Done
  RenderObject::initialize();
  QString fileExtension = mGsLocation.split(".").last();
  QString splatExt = QString("splat");
  if (fileExtension.contains(splatExt)) {
    LoadSplatGs(mGsLocation);
    SPDLOG_INFO("splat file extension load");
  } else if (fileExtension.contains("vsplat"))
    LoadAnimateGs(mGsLocation);
  else if (fileExtension.contains("ply"))
    LoadGaussianCloud(mGsLocation);
  else {
    LoadSplatGs(mGsLocation);
    SPDLOG_ERROR("file extension wrong:{} ", fileExtension.toStdString());
  }

}

void GsRenderObject::LoadSplatGs(const QString& location) {
  // Clear previous data
    std::vector<unsigned char> data = readFromFile(location.toStdString());

    // resize data folowing the vertexCount
    vertexCount = static_cast<int>(data.size() / rowLength);

    depthIndex.resize(vertexCount + 1);

    texheight = std::ceil((float)(2 * vertexCount) / (float)texwidth);  // Set to your desired height
    m_data = std::make_unique<SplatData>(data);
    textureCoro.setData(std::make_unique<SplatData>(data));
    textureCoro.generateTexture();
    initializeGL(); 
    setView(viewMatrix[std::array{0, 2}], viewMatrix[std::array{1, 2}],
            viewMatrix[std::array{2, 2}]);
} 

void GsRenderObject::LoadAnimateGs(const QString& location) {}
  
void GsRenderObject::draw() {
 // worldInteraction(viewMatrix);
  setView(viewMatrix[std::array{0, 2}], viewMatrix[std::array{1, 2}],  viewMatrix[std::array{2, 2}]);
}
//
//void GsRenderObject::viewChanged() { 
//    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
//
//  // fps calculations (from paintGL)
//  f->glUniformMatrix4fv(m_viewLoc, 1, false, viewMatrix.data_handle());
//  f->glClear(GL_COLOR_BUFFER_BIT);
//  QOpenGLContext::currentContext()->extraFunctions()->glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4,
//                                                                            vertexCount);
//} 
void GsRenderObject::sortByDepth(float x, float y, float z) {
  int maxDepth = std::numeric_limits<int>::min();
  int minDepth = std::numeric_limits<int>::max();
  std::vector<unsigned int> sizeList(vertexCount);
  for (int i = 0; i < vertexCount; i++) {
    int depth = (x * m_data->m_floatBuffer[8 * i + 0] + y * m_data->m_floatBuffer[8 * i + 1] +
                 z * m_data->m_floatBuffer[8 * i + 2]) *
                4096;
    sizeList[i] = depth;

    if (depth > maxDepth) maxDepth = depth;
    if (depth < minDepth) minDepth = depth;
  } 
  constexpr int sizeSort = 256 * 256;
  // This is a 16 bit single-pass counting sort
  float depthInv = (sizeSort) / (maxDepth - minDepth);
  std::vector<int> counts0(sizeSort); 
  // normalize depth 
  std::vector<unsigned int> normalizedDepths = sizeList | std::views::transform([&](unsigned int& val) {
    val = std::floor((val - minDepth) * depthInv);
    counts0[val]++;  // count occurrences
    return val;
  }) | std::ranges::to<std::vector>(); 
  std::vector<int> starts0(sizeSort);
  for (int i = 1; i < sizeSort; i++) starts0[i] = starts0[i - 1] + counts0[i - 1];

  for (int i = 0; i < vertexCount; i++) {
    depthIndex[starts0[sizeList[i]]++] = i;
  } 
  setDepthIndex(depthIndex);
}


void GsRenderObject::resizeGL(int w, int h) {
  QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
  GLfloat tabFloat[] = {static_cast<GLfloat>(focalWidth), static_cast<GLfloat>(focalHeight)};
  f->glUniform2fv(m_focalLoc, 1, tabFloat);
  m_projectionMatrix = getProjectionMatrix(focalWidth, focalHeight, w, h);
  GLfloat innerTab[] = {static_cast<GLfloat>(w), static_cast<GLfloat>(h)};
  f->glUniform2fv(m_viewPortLoc, 1, innerTab);
  f->glViewport(0, 0, w, h);
  f->glUniformMatrix4fv(m_projMatrixLoc, 1, false, m_projectionMatrix.data());
}

 void GsRenderObject::initializeGL() {
  // QOpenGLDebugLogger* logger = new QOpenGLDebugLogger(this);
  // connect(logger, &QOpenGLDebugLogger::messageLogged, [&](const QOpenGLDebugMessage&
  // debugMessage) { qCritical() << debugMessage; }); logger->initialize(); // initializes in
  // the current context, i.e. ctx logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
  QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
  //  m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, ShaderSource::vertex);
  //  m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, ShaderSource::fragment);
  m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                             ":/resources/shaders/AnimateGS.vert");
  m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                             ":/resources/shaders/AnimateGS.frag");
  m_program.link();
  m_program.bind();
  // Create a VAO. Not strictly required for ES 3, but it is for plain OpenGL.
  if (m_vao.create()) m_vao.bind();
  f->glDisable(GL_DEPTH_TEST);  // Disable depth testing
  f->glEnable(GL_BLEND);
  f->glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  m_projMatrixLoc = m_program.uniformLocation("projection");
  m_viewPortLoc = m_program.uniformLocation("viewport");
  m_focalLoc = m_program.uniformLocation("focal");
  m_viewLoc = m_program.uniformLocation("view");
  // positions
  const std::array<float, 8> triangleVertices = {-2, -2, 2, -2, 2, 2, -2, 2};
  m_vertexBuffer.create();
  f->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.bufferId());
  f->glBufferData(GL_ARRAY_BUFFER, 8 * 4, triangleVertices.data(), GL_STATIC_DRAW);
  const int a_position = m_program.attributeLocation("position");
  f->glEnableVertexAttribArray(a_position);
  f->glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.bufferId());
  f->glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  f->glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
  auto u_textureLocation = m_program.uniformLocation("u_texture");
  f->glUniform1i(u_textureLocation, 0);
  m_indexBuffer.create();
  const int a_index = m_program.attributeLocation("index");
  f->glEnableVertexAttribArray(a_index);
  f->glBindBuffer(GL_ARRAY_BUFFER, m_indexBuffer.bufferId());
  f->glVertexAttribIPointer(a_index, 1, GL_INT, false, 0);
  f->glVertexAttribDivisor(a_index, 1);
}

void GsRenderObject::setTextureData(const std::vector<unsigned int>& texdata, int texwidth,
                                    int texheight) {
  QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
  f->glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  f->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, texwidth, texheight, 0, GL_RGBA_INTEGER,
                  GL_UNSIGNED_INT, texdata.data());
  f->glActiveTexture(GL_TEXTURE0);
  f->glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
}
int GsRenderObject::floatToHalf(float val) {
  unsigned int f;
  memcpy(&f, &val, 4);
  int sign = (f >> 31) & 0x0001;
  int exp = (f >> 23) & 0x00ff;
  int frac = f & 0x007fffff;
  int newExp = 0;
  if (exp < 113) {
    newExp = 0;
    frac |= 0x00800000;
    frac = frac >> (113 - exp);
    if (frac & 0x01000000) {
      newExp = 1;
      frac = 0;
    }
  } else if (exp < 142) {
    newExp = exp - 112;
  } else {
    newExp = 31;
    frac = 0;
  }
  return (sign << 15) | (newExp << 10) | (frac >> 13);
}
void GsRenderObject::rotateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4> > matrix,
                                  float rad, float x,
                  float y, float z) {
  float len = std::hypot(x, y, z);
  x /= len;
  y /= len;
  z /= len;
  float s = std::sin(rad);
  float c = std::cos(rad);
  float t = 1 - c;
  float b00 = x * x * t + c;
  float b01 = y * x * t + z * s;
  float b02 = z * x * t - y * s;
  float b10 = x * y * t - z * s;
  float b11 = y * y * t + c;
  float b12 = z * y * t + x * s;
  float b20 = x * z * t + y * s;
  float b21 = y * z * t - x * s;
  float b22 = z * z * t + c;
  matrix[std::array{0, 0}] = matrix[std::array{0, 0}] * b00 + matrix[std::array{1, 0}] * b01 +
                             matrix[std::array{2, 0}] * b02;
  matrix[std::array{0, 1}] = matrix[std::array{0, 1}] * b00 + matrix[std::array{1, 1}] * b01 +
                             matrix[std::array{2, 1}] * b02;
  matrix[std::array{0, 2}] = matrix[std::array{0, 2}] * b00 + matrix[std::array{1, 2}] * b01 +
                             matrix[std::array{2, 2}] * b02;
  matrix[std::array{0, 3}] = matrix[std::array{0, 3}] * b00 + matrix[std::array{1, 3}] * b01 +
                             matrix[std::array{2, 3}] * b02;

  matrix[std::array{1, 0}] = matrix[std::array{0, 0}] * b10 + matrix[std::array{1, 0}] * b11 +
                             matrix[std::array{2, 0}] * b12;
  matrix[std::array{1, 1}] = matrix[std::array{0, 1}] * b10 + matrix[std::array{1, 1}] * b11 +
                             matrix[std::array{2, 1}] * b12;
  matrix[std::array{1, 2}] = matrix[std::array{0, 2}] * b10 + matrix[std::array{1, 2}] * b11 +
                             matrix[std::array{2, 2}] * b12;
  matrix[std::array{1, 3}] = matrix[std::array{0, 3}] * b10 + matrix[std::array{1, 3}] * b11 +
                             matrix[std::array{2, 3}] * b12;

  matrix[std::array{2, 0}] = matrix[std::array{0, 0}] * b20 + matrix[std::array{1, 0}] * b21 +
                             matrix[std::array{2, 0}] * b22;
  matrix[std::array{2, 1}] = matrix[std::array{0, 1}] * b20 + matrix[std::array{1, 1}] * b21 +
                             matrix[std::array{2, 1}] * b22;
  matrix[std::array{2, 2}] = matrix[std::array{0, 2}] * b20 + matrix[std::array{1, 2}] * b21 +
                             matrix[std::array{2, 2}] * b22;
  matrix[std::array{2, 3}] = matrix[std::array{0, 3}] * b20 + matrix[std::array{1, 3}] * b21 +
                             matrix[std::array{2, 3}] * b22;
}
void GsRenderObject::translateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4> > matrix, float x, float y, float z) {
  matrix[std::array{3, 0}] +=
      matrix[std::array{0, 0}] * x + matrix[std::array{1, 0}] * y + matrix[std::array{2, 0}] * z;
  matrix[std::array{3, 1}] +=
      matrix[std::array{0, 1}] * x + matrix[std::array{1, 1}] * y + matrix[std::array{2, 1}] * z;
  matrix[std::array{3, 2}] +=
      matrix[std::array{0, 2}] * x + matrix[std::array{1, 2}] * y + matrix[std::array{2, 2}] * z;
  matrix[std::array{3, 3}] +=
      matrix[std::array{0, 3}] * x + matrix[std::array{1, 3}] * y + matrix[std::array{2, 3}] * z;
}

}  // namespace nimagna
