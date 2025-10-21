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
#include <array>
#include <cmath>
#include <mdspan>
#include <memory>
#include <type_traits>
#include <vector>

namespace nimagna {

GsRenderObject::GsRenderObject(const QString& location)
    : m_texture(QOpenGLTexture::Target2D), mGsLocation(location) {
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

std::vector<unsigned char> GsRenderObject::readFromFile(const std::filesystem::path& path) {
  // file buffer
  std::vector<unsigned char> u_buffer(std::filesystem::file_size(path));
  // read file data to buffer
  std::basic_ifstream<unsigned char> inputFile(path, std::ios_base::binary);
  inputFile.read(u_buffer.data(), u_buffer.size());
  inputFile.close();
  return u_buffer;
}

void GsRenderObject::LoadSplatGs(const QString& location) {
  SPDLOG_INFO("in LoadSplatGs");
  // Clear previous data
  std::vector<unsigned char> data = readFromFile(location.toStdString()); 
  // resize data folowing the vertexCount
  vertexCount = static_cast<int>(data.size() / rowLength); 
  depthIndex.resize(vertexCount + 1); 
  texheight = std::ceil((float)(2 * vertexCount) / (float)texwidth);  // Set to your desired height
  m_data = std::make_unique<SplatData>(data);
  initializeGL(); 
  isDataReady = true; 
  RenderObject::initialize();
}

void GsRenderObject::LoadAnimateGs(const QString& location) {}
 
void GsRenderObject::sortByDepth(QVector3D cameraPosition) {
  int maxDepth = std::numeric_limits<int>::min();
  int minDepth = std::numeric_limits<int>::max();
  std::vector<unsigned int> sizeList(vertexCount);
  const auto x = cameraPosition.x();
  const auto y = cameraPosition.y();
  const auto z = cameraPosition.z();
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
  std::vector<unsigned int> normalizedDepths = sizeList |
                                               std::views::transform([&](unsigned int& val) {
                                                 val = std::floor((val - minDepth) * depthInv);
                                                 counts0[val]++;  // count occurrences
                                                 return val;
                                               }) |
                                               std::ranges::to<std::vector>();
  std::vector<int> starts0(sizeSort);
  for (int i = 1; i < sizeSort; i++) starts0[i] = starts0[i - 1] + counts0[i - 1];

  for (int i = 0; i < vertexCount; i++) {
    depthIndex[starts0[sizeList[i]]++] = i;
  } 
  glBindBuffer(GL_ARRAY_BUFFER, m_indexBuffer.bufferId());
  glBufferData(GL_ARRAY_BUFFER, depthIndex.size() * 4, depthIndex.data(), GL_DYNAMIC_DRAW);
  SPDLOG_INFO("setDepthIndex done");
}
 
void GsRenderObject::initializeGL() {  
  m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, ":/resources/shaders/AnimateGS.vert");
  m_program.addCacheableShaderFromSourceFile(QOpenGLShader::Fragment, ":/resources/shaders/AnimateGS.frag");
  m_program.link();
  m_program.bind();
  // Create a VAO. Not strictly required for ES 3, but it is for plain OpenGL.
  if (m_vao.create()) m_vao.bind();
  glDisable(GL_DEPTH_TEST);  // Disable depth testing
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  m_projMatrixLoc = m_program.uniformLocation("projection");
  m_viewPortLoc = m_program.uniformLocation("viewport");
  m_focalLoc = m_program.uniformLocation("focal");
  m_viewLoc = m_program.uniformLocation("view");
  // positions
  const std::array<float, 8> triangleVertices = {-2, -2, 2, -2, 2, 2, -2, 2};
  m_vertexBuffer.create();
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.bufferId());
  glBufferData(GL_ARRAY_BUFFER, 8 * 4, triangleVertices.data(), GL_STATIC_DRAW);
  const int a_position = m_program.attributeLocation("position");
  glEnableVertexAttribArray(a_position);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer.bufferId());
  glVertexAttribPointer(a_position, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
  auto u_textureLocation = m_program.uniformLocation("u_texture");
  glUniform1i(u_textureLocation, 0);
  m_indexBuffer.create();
  const int a_index = m_program.attributeLocation("index");
  glEnableVertexAttribArray(a_index);
  glBindBuffer(GL_ARRAY_BUFFER, m_indexBuffer.bufferId());
  glVertexAttribIPointer(a_index, 1, GL_INT, false, 0);
  glVertexAttribDivisor(a_index, 1);
}

void GsRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  if (!isDataReady) {
    return;
  }

  // determine the current position in the view and its change compared to the last update
  const auto inverseViewMatrix = viewMatrix.inverted();
  auto cameraPositionRaw = inverseViewMatrix.column(3);
  cameraPositionRaw /= cameraPositionRaw.w();
  const auto cameraPosition = cameraPositionRaw.toVector3D();
  const auto dotProduct = QVector3D::dotProduct(mLastCameraPosition, cameraPosition);
  if (std::abs(dotProduct - 1) > 0.01) {
    // position changed -> update
    std::optional<std::vector<unsigned int>> texdata = generateTexture();

    if (texdata.has_value()) {
      setTextureData(texdata.value(), texwidth, texheight);
    }
    sortByDepth(cameraPosition);
    mLastCameraPosition = cameraPosition;
  }

  // update program
  m_program.bind();
  checkOpenGLError("m_program.bind");

  QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
  f->glUniformMatrix4fv(m_viewLoc, 1, false, viewMatrix.constData());
  checkOpenGLError("glUniformMatrix4fv");
  f->glDisable(GL_DEPTH_TEST);
  checkOpenGLError("glDisable(GL_DEPTH_TEST)");
  f->glEnable(GL_BLEND);
  checkOpenGLError("glEnable(GL_BLEND)");
  f->glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  checkOpenGLError("glBlendFuncSeparate");
  f->glClear(GL_COLOR_BUFFER_BIT);
  checkOpenGLError("glClear");

  // bind VAO and draw  
  m_vao.bind();
  checkOpenGLError("m_vao.bind");
  f->glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, vertexCount);
  checkOpenGLError("glDrawArraysInstanced");
  // m_program.release();
  checkOpenGLError("m_program.release");
}

void GsRenderObject::setTextureData(const std::vector<unsigned int>& texdata, int texwidth,
                                    int texheight) {
  glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, texwidth, texheight, 0, GL_RGBA_INTEGER,
                  GL_UNSIGNED_INT, texdata.data());
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_texture.textureId());
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
void GsRenderObject::rotateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix,
                                  float rad, float x, float y, float z) {
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
void GsRenderObject::translateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix,
                                     float x, float y, float z) {
  matrix[std::array{3, 0}] +=
      matrix[std::array{0, 0}] * x + matrix[std::array{1, 0}] * y + matrix[std::array{2, 0}] * z;
  matrix[std::array{3, 1}] +=
      matrix[std::array{0, 1}] * x + matrix[std::array{1, 1}] * y + matrix[std::array{2, 1}] * z;
  matrix[std::array{3, 2}] +=
      matrix[std::array{0, 2}] * x + matrix[std::array{1, 2}] * y + matrix[std::array{2, 2}] * z;
  matrix[std::array{3, 3}] +=
      matrix[std::array{0, 3}] * x + matrix[std::array{1, 3}] * y + matrix[std::array{2, 3}] * z;
}
std::vector<unsigned int> GsRenderObject::generateTexture() {
  std::vector<unsigned int> texdata;

  // Here we convert from a .splat file buffer into a texture
  // With a little bit more foresight perhaps this texture file
  // should have been the native format as it'd be very easy to
  // load it into webgl.

  int texwidth = 2048;
  int texheight;
  texheight = std::ceil((float)(2 * vertexCount) / (float)texwidth);  // Set to your desired height
  texdata.resize(texwidth * texheight * 4);

  // For reinterpretation
  float* texdata_f = reinterpret_cast<float*>(texdata.data());
  uint8_t* texdata_c = reinterpret_cast<uint8_t*>(texdata.data());

  for (int i = 0; i < vertexCount; ++i) {
    // x, y, z
    texdata_f[8 * i + 0] = m_data->m_floatBuffer[8 * i + 0];
    texdata_f[8 * i + 1] = m_data->m_floatBuffer[8 * i + 1];
    texdata_f[8 * i + 2] = m_data->m_floatBuffer[8 * i + 2];

    // r, g, b, a
    texdata_c[4 * (8 * i + 7) + 0] = m_data->m_ucharBuffer[32 * i + 24 + 0];
    texdata_c[4 * (8 * i + 7) + 1] = m_data->m_ucharBuffer[32 * i + 24 + 1];
    texdata_c[4 * (8 * i + 7) + 2] = m_data->m_ucharBuffer[32 * i + 24 + 2];
    texdata_c[4 * (8 * i + 7) + 3] = m_data->m_ucharBuffer[32 * i + 24 + 3];

    // scale
    float scale[3] = {m_data->m_floatBuffer[8 * i + 3], m_data->m_floatBuffer[8 * i + 4],
                      m_data->m_floatBuffer[8 * i + 5]};

    // quaternion
    float rot[4] = {(static_cast<int>(m_data->m_ucharBuffer[32 * i + 28 + 0]) - 128) / 128.0f,
                    (static_cast<int>(m_data->m_ucharBuffer[32 * i + 28 + 1]) - 128) / 128.0f,
                    (static_cast<int>(m_data->m_ucharBuffer[32 * i + 28 + 2]) - 128) / 128.0f,
                    (static_cast<int>(m_data->m_ucharBuffer[32 * i + 28 + 3]) - 128) / 128.0f};

    // Compute the matrix product of S and R (M = S * R)
    float M[9] = {1.0f - 2.0f * (rot[2] * rot[2] + rot[3] * rot[3]),
                  2.0f * (rot[1] * rot[2] + rot[0] * rot[3]),
                  2.0f * (rot[1] * rot[3] - rot[0] * rot[2]),

                  2.0f * (rot[1] * rot[2] - rot[0] * rot[3]),
                  1.0f - 2.0f * (rot[1] * rot[1] + rot[3] * rot[3]),
                  2.0f * (rot[2] * rot[3] + rot[0] * rot[1]),

                  2.0f * (rot[1] * rot[3] + rot[0] * rot[2]),
                  2.0f * (rot[2] * rot[3] - rot[0] * rot[1]),
                  1.0f - 2.0f * (rot[1] * rot[1] + rot[2] * rot[2])};
    for (int j = 0; j < 9; ++j) {
      M[j] *= scale[j / 3];
    }

    float sigma[6] = {
        M[0] * M[0] + M[3] * M[3] + M[6] * M[6], M[0] * M[1] + M[3] * M[4] + M[6] * M[7],
        M[0] * M[2] + M[3] * M[5] + M[6] * M[8], M[1] * M[1] + M[4] * M[4] + M[7] * M[7],
        M[1] * M[2] + M[4] * M[5] + M[7] * M[8], M[2] * M[2] + M[5] * M[5] + M[8] * M[8]};

    texdata[8 * i + 4] = packHalf2x16(4 * sigma[0], 4 * sigma[1]);
    texdata[8 * i + 5] = packHalf2x16(4 * sigma[2], 4 * sigma[3]);
    texdata[8 * i + 6] = packHalf2x16(4 * sigma[4], 4 * sigma[5]);
  }
  return texdata;
}
void GsRenderObject::invertMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix) {
  float b00 = matrix[std::array{0, 0}] * matrix[std::array{1, 1}] -
              matrix[std::array{0, 1}] * matrix[std::array{1, 0}];
  float b01 = matrix[std::array{0, 0}] * matrix[std::array{1, 2}] -
              matrix[std::array{0, 2}] * matrix[std::array{1, 0}];
  float b02 = matrix[std::array{0, 0}] * matrix[std::array{1, 3}] -
              matrix[std::array{0, 3}] * matrix[std::array{1, 0}];
  float b03 = matrix[std::array{0, 1}] * matrix[std::array{1, 2}] -
              matrix[std::array{0, 2}] * matrix[std::array{1, 1}];

  float b04 = matrix[std::array{0, 1}] * matrix[std::array{1, 3}] -
              matrix[std::array{0, 3}] * matrix[std::array{1, 1}];
  float b05 = matrix[std::array{0, 2}] * matrix[std::array{1, 3}] -
              matrix[std::array{0, 3}] * matrix[std::array{1, 2}];
  float b06 = matrix[std::array{2, 0}] * matrix[std::array{3, 1}] -
              matrix[std::array{2, 1}] * matrix[std::array{3, 0}];
  float b07 = matrix[std::array{2, 0}] * matrix[std::array{3, 2}] -
              matrix[std::array{2, 2}] * matrix[std::array{3, 0}];

  float b08 = matrix[std::array{2, 0}] * matrix[std::array{3, 3}] -
              matrix[std::array{2, 3}] * matrix[std::array{3, 0}];
  float b09 = matrix[std::array{2, 1}] * matrix[std::array{3, 2}] -
              matrix[std::array{2, 2}] * matrix[std::array{3, 1}];
  float b10 = matrix[std::array{2, 1}] * matrix[std::array{3, 3}] -
              matrix[std::array{2, 3}] * matrix[std::array{3, 1}];
  float b11 = matrix[std::array{2, 2}] * matrix[std::array{3, 3}] -
              matrix[std::array{2, 3}] * matrix[std::array{3, 2}];
  float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
  if (det < 0.0000001) {
    return;
  }
  std::array<float, 16> arr{(matrix[std::array{1, 1}] * b11 - matrix[std::array{1, 2}] * b10 +
                             matrix[std::array{1, 3}] * b09) /
                                det,
                            (matrix[std::array{0, 2}] * b10 - matrix[std::array{0, 1}] * b11 -
                             matrix[std::array{0, 3}] * b09) /
                                det,
                            (matrix[std::array{3, 1}] * b05 - matrix[std::array{3, 2}] * b04 +
                             matrix[std::array{3, 3}] * b03) /
                                det,
                            (matrix[std::array{2, 2}] * b04 - matrix[std::array{2, 1}] * b05 -
                             matrix[std::array{2, 3}] * b03) /
                                det,

                            (matrix[std::array{1, 2}] * b08 - matrix[std::array{1, 0}] * b11 -
                             matrix[std::array{1, 3}] * b07) /
                                det,
                            (matrix[std::array{0, 0}] * b11 - matrix[std::array{0, 2}] * b08 +
                             matrix[std::array{0, 3}] * b07) /
                                det,
                            (matrix[std::array{3, 2}] * b02 - matrix[std::array{3, 0}] * b05 -
                             matrix[std::array{3, 3}] * b01) /
                                det,
                            (matrix[std::array{2, 0}] * b05 - matrix[std::array{2, 2}] * b02 +
                             matrix[std::array{2, 3}] * b01) /
                                det,

                            (matrix[std::array{1, 0}] * b10 - matrix[std::array{1, 1}] * b08 +
                             matrix[std::array{1, 3}] * b06) /
                                det,
                            (matrix[std::array{0, 1}] * b08 - matrix[std::array{0, 0}] * b10 -
                             matrix[std::array{0, 3}] * b06) /
                                det,
                            (matrix[std::array{3, 0}] * b04 - matrix[std::array{3, 1}] * b02 +
                             matrix[std::array{3, 3}] * b00) /
                                det,
                            (matrix[std::array{2, 1}] * b02 - matrix[std::array{2, 0}] * b04 -
                             matrix[std::array{2, 3}] * b00) /
                                det,

                            (matrix[std::array{1, 1}] * b07 - matrix[std::array{1, 0}] * b09 -
                             matrix[std::array{1, 2}] * b06) /
                                det,
                            (matrix[std::array{0, 0}] * b09 - matrix[std::array{0, 1}] * b07 +
                             matrix[std::array{0, 2}] * b06) /
                                det,
                            (matrix[std::array{3, 1}] * b01 - matrix[std::array{3, 0}] * b03 -
                             matrix[std::array{3, 2}] * b00) /
                                det,
                            (matrix[std::array{2, 0}] * b03 - matrix[std::array{2, 1}] * b01 +
                             matrix[std::array{2, 2}] * b00) /
                                det};

  std::memcpy(matrix.data_handle(), arr.data(), sizeof(float) * 16);
}
}  // namespace nimagna
