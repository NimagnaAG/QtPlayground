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
#include "Rendering/Rendering.h"
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
  std::vector<unsigned char> readFromFile(const std::filesystem::path& path) {
    // file buffer
    std::vector<unsigned char> u_buffer(std::filesystem::file_size(path));
    // read file data to buffer
    std::basic_ifstream<unsigned char> inputFile(path, std::ios_base::binary);
    inputFile.read(u_buffer.data(), u_buffer.size());
    inputFile.close();
    return u_buffer;
  }

  void LoadSplatGs(const QString& location);
  void LoadAnimateGs(const QString& location);
  void LoadGaussianCloud(const QString& location) {}
  // initializes the render object.
  virtual void initialize() override;
  // draws the render object.
  virtual void draw() override; 
  bool isControl = false;
  virtual void keyPressEvent(QKeyEvent* event) override {
    SPDLOG_INFO("keyPressEvent");
    if (event->type() == QEvent::KeyPress) { 
      invertMatrix(viewMatrix); 
      if (event->key() == Qt::Key_Up ) {
        translateMatrix(viewMatrix, 0, 0, 0.25);
      } else if (event->key() == Qt::Key_Down  ) {
        translateMatrix(viewMatrix, 0, 0, -0.25);
      } else if (event->key() == Qt::Key_Left) {
        translateMatrix(viewMatrix, -0.25, 0, 0);
      } else if (event->key() == Qt::Key_Right) {
        translateMatrix(viewMatrix, 0.25, 0, 0);
      } else if (event->key() == Qt::Key_R) {
        isControl = false;
      } else if (event->key() == Qt::Key_W) {
        rotateMatrix(viewMatrix,0.1f, 0, 0, 0.25f);
      } else if (event->key() == Qt::Key_S) {
        rotateMatrix(viewMatrix,0.1f, 0, 0, -0.25f);
      } 
      isControl = true;
      invertMatrix(viewMatrix); 
    }  
  }; 
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
  QString mGsLocation = "";
  int vertexCount = 0;
  int LastVertexCount = -1;
  std::unique_ptr<SplatData> m_data;
  float lastProjX, lastProjY, lastProjZ;
  bool isDataReady = false;
  static constexpr int texwidth = 2048;
  int texheight;
  std::vector<unsigned int> depthIndex; 
 protected:
  std::array<float, 16> getProjectionMatrix(float fx, float fy, int width, int height) {
    constexpr float znear = 0.2f;
    constexpr float zfar = 200;
    return {(2.0f * fx) / width,  0.f, 0.f, 0.f,
            0.f, -(2 * fy) / height, 0.f, 0.f,
            0.f, 0.f, zfar / (zfar - znear), 1.f,
            0.f, 0.f,  -(zfar * znear) / (zfar - znear), 0.f};
  }

  int floatToHalf(float val);
  void rotateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix, float rad, float x,
                    float y, float z);
  void translateMatrix(std::mdspan<float, std::extents<std::size_t, 4, 4>> matrix, float x, float y,
                       float z);
  
  unsigned int packHalf2x16(float x, float y) {
    return std::bitset<32>(floatToHalf(x) | floatToHalf(y) << 16).to_ulong();
  }
  std::mdspan<float, std::extents<std::size_t, 4, 4>> viewMatrix;
        
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

    // Example usage in viewChanged()
    void viewChanged() {
        m_program.bind();

        QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
        f->glUniformMatrix4fv(m_viewLoc, 1, false, viewMatrix.data_handle());
        checkOpenGLError("glUniformMatrix4fv");
        f->glDisable(GL_DEPTH_TEST);
        checkOpenGLError("glDisable(GL_DEPTH_TEST)");
        f->glEnable(GL_BLEND);
        checkOpenGLError("glEnable(GL_BLEND)");
        f->glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ONE_MINUS_DST_ALPHA, GL_ONE); 
        checkOpenGLError("glBlendFuncSeparate");
        f->glClear(GL_COLOR_BUFFER_BIT); 
        checkOpenGLError("glClear");
        
        checkOpenGLError("m_program.bind");
        m_vao.bind();
        checkOpenGLError("m_vao.bind");
        f->glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, vertexCount);
        checkOpenGLError("glDrawArraysInstanced");
       // m_program.release();
        checkOpenGLError("m_program.release");
        
  }
  void initializeGL();
  void sortByDepth(float x, float y, float z);
  virtual void resizeGL(int w, int h) override;

  void setTextureData(const std::vector<unsigned int>& texdata, int texwidth, int texheight);

  void setDepthIndex(const std::vector<unsigned int>& depthIndex) {
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindBuffer(GL_ARRAY_BUFFER, m_indexBuffer.bufferId());
    f->glBufferData(GL_ARRAY_BUFFER, depthIndex.size() * 4, depthIndex.data(), GL_DYNAMIC_DRAW);
    SPDLOG_INFO("setDepthIndex done");
  }
  void setView(float x, float y, float z) {
    float dot = lastProjX * x + lastProjY * y + lastProjZ * z;
    if (std::abs(dot - 1) > 0.01) {
      std::optional<std::vector<unsigned int>> texdata = generateTexture();
           
      if (texdata.has_value()) {
         setTextureData(texdata.value(), texwidth, texheight);
      } 
      sortByDepth(x, y, z); 
      lastProjX = x;
      lastProjY = y;
      lastProjZ = z;
      
    }
    viewChanged();
  }  
};

}  // namespace nimagna
