#pragma once
#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QOpenGLExtraFunctions>
#include <QVector>
#include <QtCore/QMutex>
#include <QtCore/QSize>
#include <QtGlobal>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <array>
#include <bit>
#include <bitset>
#include <chrono>
#include <cmath>
#include <coroutine>
#include <cstring>
#include <expected>
#include <fstream>
#include <future>
#include <limits>
#include <mdspan>
#include <memory>
#include <ranges>
#include <thread>
#include <vector>

#include "RenderObject.h"
#include "Renderer.h" 
namespace nimagna {

class OpenGlWidget;
class RENDERING_API GsRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT

  friend class OpenGlWidget;

 public:
  GsRenderObject() = delete;
  GsRenderObject(const QString& location);
  // not copyable or movable
  GsRenderObject(const GsRenderObject& other) = delete;
  GsRenderObject& operator=(const GsRenderObject& other) = delete;
  GsRenderObject(GsRenderObject&&) = delete;
  GsRenderObject& operator=(GsRenderObject&&) = delete;
  virtual ~GsRenderObject() {}
  // initializes the render object.
  virtual void initialize() override;
  // draws the render object.
  virtual void draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) override;
  virtual void keyPressEvent(QKeyEvent* event) override {};
  virtual void resizeGL(int w, int h) override {};
 private:
  std::vector<unsigned char> readFromFile(const std::filesystem::path& path);

  void LoadSplatGs(const QString& location);
  void LoadAnimateGs(const QString& location);
  void LoadGaussianCloud(const QString& location) {}

  bool isControl = false;

  static unsigned int to_uints(float v) { return std::bit_cast<unsigned int>(v); }
  struct SplatData {
    SplatData(const std::vector<unsigned char> splatBuffer)
        : m_ucharBuffer(splatBuffer), m_floatBuffer(m_ucharBuffer.size() / 4) {
      std::memcpy(m_floatBuffer.data(), m_ucharBuffer.data(),
                  m_ucharBuffer.size());  // copy binary to float with our friend memcpy!
      m_uintBuffer =
          m_floatBuffer | std::views::transform([this](float v) { return to_uints(v); }) |
          std::ranges::to<std::vector<unsigned int>>();  // convert float to uint using ranges!
    }
    std::vector<unsigned char> m_ucharBuffer;
    std::vector<float> m_floatBuffer;
    std::vector<unsigned int> m_uintBuffer;
  };

  std::vector<unsigned int> generateTexture();
  const QString mGsLocation;
  int vertexCount = 0;
  int LastVertexCount = -1;
  std::unique_ptr<SplatData> m_data;
  QVector3D mLastCameraPosition;
  bool isDataReady = false;
  static constexpr int texwidth = 2048;
  int texheight;
  std::vector<unsigned int> depthIndex;

 protected:
  std::array<float, 16> getProjectionMatrix(float fx, float fy, int width, int height) {
    constexpr float znear = 0.2f; constexpr float zfar = 200;
    return {(2.0f * fx) / width, 0.f, 0.f, 0.f, 
             0.f,  -(2 * fy) / height, 0.f,  0.f, 
             0.f,  0.f, zfar / (zfar - znear), 1.f,
            0.f,  0.f,  -(zfar * znear) / (zfar - znear),  0.f};
  }

  int floatToHalf(float val);
  void rotateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix, float rad, float x,
                    float y, float z);
  void translateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix, float x, float y,
                       float z);

  unsigned int packHalf2x16(float x, float y) {
    return std::bitset<32>(floatToHalf(x) | floatToHalf(y) << 16).to_ulong();
  }

  int focalWidth = 1500;
  int focalHeight = 1500;

  int rowLength = 32;
  QOpenGLTexture m_texture;
  QOpenGLShaderProgram m_program;
  QOpenGLVertexArrayObject m_vao;
  int m_projMatrixLoc = 0;
  int m_viewPortLoc = 0;
  int m_focalLoc = 0;
  int m_viewLoc = 0;
  QOpenGLBuffer m_indexBuffer;
  QOpenGLBuffer m_vertexBuffer;
  void invertMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix);
  std::array<float, 16> m_projectionMatrix;
  void checkOpenGLError(const char* location) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
      qWarning() << "OpenGL error at" << location << ":" << err;
    }
  }

  void initializeGL();
  void sortByDepth(QVector3D cameraPosition);

  void setTextureData(const std::vector<unsigned int>& texdata, int texwidth, int texheight);

  void setDepthIndex(const std::vector<unsigned int>& depthIndex);

};

}  // namespace nimagna
