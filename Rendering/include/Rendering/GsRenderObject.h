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
  struct TextureGenerator {
    std::thread myThread;
    struct promise_type {
      std::vector<unsigned int> textureData{};  // what the coroutine produces
      std::unique_ptr<SplatData> m_splatData;   // what it needs to produce

      TextureGenerator get_return_object() noexcept {
        return TextureGenerator{this};
      }  // #C Coroutine creation
      std::suspend_always initial_suspend() noexcept { return {}; }  // #D Startup
      std::suspend_always final_suspend() noexcept { return {}; }    // #E Ending
      std::suspend_always yield_value(
          std::vector<unsigned int> data) noexcept  // #F Value from co_yield
      {
        textureData = std::move(data);
        return {};
      }
      void unhandled_exception() noexcept {}
      void return_void() noexcept {}

      auto await_transform(std::unique_ptr<SplatData>) noexcept  // G Value from co_await 14
      {
        struct awaiter {
          // Customized version instead of using suspend_always or suspend_never
          promise_type& pt;
          constexpr bool await_ready() const noexcept { return true; }
          std::unique_ptr<SplatData> await_resume() const noexcept {
            return std::move(pt.m_splatData);
          }
          void await_suspend(std::coroutine_handle<>) const noexcept {}
        };
        return awaiter{*this};
      }
    };

    std::coroutine_handle<promise_type> co_handle{};
    explicit TextureGenerator(promise_type* p)
        : myThread(),
          co_handle{std::coroutine_handle<promise_type>::from_promise(*p)}
    // #C Get the handle form the promise
    {}
    TextureGenerator(TextureGenerator&& rhs) noexcept
        : co_handle{std::exchange(rhs.co_handle, nullptr)} {}  // #D Move only!

    ~TextureGenerator() noexcept  // #E Care taking, destroying the handle if needed
    {
      myThread.join();
      if (co_handle) {
        co_handle.destroy();
      }
    }

    void setData(std::unique_ptr<SplatData> data) {
      co_handle.promise().m_splatData = std::move(data);
    }

    void generateTexture()  // #F Activate the coroutine and
    {
      if (!myThread.joinable()) {
        myThread = std::move(std::thread(([this] {
          if (not co_handle.done()) {
            co_handle.resume();
          }
        })));
      }
    }
    enum class TextureStatus { NotReady, NoData };
    std::optional<std::vector<unsigned int>> texture()  //  return the data
    {
      if (co_handle.done()) return std::nullopt;

      if (co_handle.promise().textureData.size() == 0) return std::nullopt;

      return std::move(co_handle.promise().textureData);
    }
  };
  TextureGenerator TextureCoroutine() {
    std::vector<unsigned int> texdata;

    // Here we convert from a .splat file buffer into a texture
    // With a little bit more foresight perhaps this texture file
    // should have been the native format as it'd be very easy to
    // load it into webgl.

    int texwidth = 2048;
    int texheight;
    std::unique_ptr<SplatData> m_data = co_await std::unique_ptr<SplatData>{};

    
    texheight =
        std::ceil((float)(2 * vertexCount) / (float)texwidth);  // Set to your desired height
    texdata.resize(texwidth * texheight * 4);

    /*
    std::array<float, 4> rot;
    std::array<float, 9> M;
    std::array<float, 6> sigma;
    while (true) {
            //auto spanny = std::mdspan(m_data->m_ucharBuffer.data(), 32,
    m_data->m_floatBuffer.size());

            for (unsigned int i : std::views::iota(0u, m_data->m_floatBuffer.size()) |
    std::views::stride(8)) {
                    // x, y, z from float to binary
                    //texdata[i]     = uintBuffer[i];
                    //texdata[i + 1] = uintBuffer[i + 1];
                    //texdata[i + 2] = uintBuffer[i + 2];
                    std::ranges::copy_n(m_data->m_uintBuffer.begin() + i, 3, texdata.begin() + i);

                    // r, g, b, a
                    std::memcpy(&texdata[i + 7], &m_data->m_ucharBuffer[4 * i + 24], 4);

                    // quaternions

                    rot[0] = std::bit_cast<float>(m_data->m_ucharBuffer[4 * i + 28 + 0] - 128.0f) /
    128.0f; rot[1] = std::bit_cast<float>(m_data->m_ucharBuffer[4 * i + 28 + 1] - 128.0f) / 128.0f;
                    rot[2] = std::bit_cast<float>(m_data->m_ucharBuffer[4 * i + 28 + 2] - 128.0f) /
    128.0f; rot[3] = std::bit_cast<float>(m_data->m_ucharBuffer[4 * i + 28 + 3] - 128.0f) / 128.0f;

                    // Compute the matrix product of S and R (M = S * R)
                    M[0] = m_data->m_floatBuffer[i + 3 + 0] * (1.0f - 2.0f * (rot[2] * rot[2] +
    rot[3] * rot[3])); M[1] = m_data->m_floatBuffer[i + 3 + 1] * (2.0f * (rot[1] * rot[2] + rot[0] *
    rot[3])); M[2] = m_data->m_floatBuffer[i + 3 + 2] * (2.0f * (rot[1] * rot[3] - rot[0] *
    rot[2]));

                    M[3] = m_data->m_floatBuffer[i + 3 + 0] * (2.0f * (rot[1] * rot[2] - rot[0] *
    rot[3])); M[4] = m_data->m_floatBuffer[i + 3 + 1] * (1.0f - 2.0f * (rot[1] * rot[1] + rot[3] *
    rot[3])); M[5] = m_data->m_floatBuffer[i + 3 + 2] * (2.0f * (rot[2] * rot[3] + rot[0] *
    rot[1]));

                    M[6] = m_data->m_floatBuffer[i + 3 + 0] * (2.0f * (rot[1] * rot[3] + rot[0] *
    rot[2])); M[7] = m_data->m_floatBuffer[i + 3 + 1] * (2.0f * (rot[2] * rot[3] - rot[0] *
    rot[1])); M[8] = m_data->m_floatBuffer[i + 3 + 2] * (1.0f - 2.0f * (rot[1] * rot[1] + rot[2] *
    rot[2]));

                    sigma[0] = M[0] * M[0] + M[3] * M[3] + M[6] * M[6];
                    sigma[1] = M[0] * M[1] + M[3] * M[4] + M[6] * M[7];
                    sigma[2] = M[0] * M[2] + M[3] * M[5] + M[6] * M[8];
                    sigma[3] = M[1] * M[1] + M[4] * M[4] + M[7] * M[7];
                    sigma[4] = M[1] * M[2] + M[4] * M[5] + M[7] * M[8];
                    sigma[5] = M[2] * M[2] + M[5] * M[5] + M[8] * M[8];


                    texdata[i + 4] = packHalf2x16(4 * sigma[0], 4 * sigma[1]);
                    texdata[i + 5] = packHalf2x16(4 * sigma[2], 4 * sigma[3]);
                    texdata[i + 6] = packHalf2x16(4 * sigma[4], 4 * sigma[5]);
            }

            co_yield texdata;
    } */
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
    co_yield texdata;
  }
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
  QString mGsLocation = "";
  int vertexCount = 0;
  int LastVertexCount = -1;
  std::unique_ptr<SplatData> m_data;
  float lastProjX, lastProjY, lastProjZ;

  static constexpr int texwidth = 2048;
  int texheight;
  std::vector<unsigned int> depthIndex;
  TextureGenerator textureCoro = TextureCoroutine();

 protected:
  std::array<float, 16> getProjectionMatrix(float fx, float fy, int width, int height) {
    constexpr float znear = 0.2f;
    constexpr float zfar = 200;
    return {(2.0f * fx) / width,
            0.f,
            0.f,
            0.f,
            0.f,
            -(2 * fy) / height,
            0.f,
            0.f,
            0.f,
            0.f,
            zfar / (zfar - znear),
            1.f,
            0.f,
            0.f,
            -(zfar * znear) / (zfar - znear),
            0.f};
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

  std::array<float, 16> m_projectionMatrix;
  void viewChanged() {
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    // fps calculations (from paintGL)
    f->glUniformMatrix4fv(m_viewLoc, 1, false, viewMatrix.data_handle());
    f->glClear(GL_COLOR_BUFFER_BIT);
    QOpenGLContext::currentContext()->extraFunctions()->glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4,
                                                                              vertexCount);
  }
  void initializeGL();
  void sortByDepth(float x, float y, float z);
  virtual void resizeGL(int w, int h) override;

  void setTextureData(const std::vector<unsigned int>& texdata, int texwidth, int texheight);

  void setDepthIndex(const std::vector<unsigned int>& depthIndex) {
    QOpenGLExtraFunctions* f = QOpenGLContext::currentContext()->extraFunctions();
    f->glBindBuffer(GL_ARRAY_BUFFER, m_indexBuffer.bufferId());
    f->glBufferData(GL_ARRAY_BUFFER, depthIndex.size() * 4, depthIndex.data(), GL_DYNAMIC_DRAW);
  }
  void setView(float x, float y, float z) {
    float dot = lastProjX * x + lastProjY * y + lastProjZ * z;
    if (std::abs(dot - 1) > 0.01) {
      std::optional<std::vector<unsigned int>> texdata =
          textureCoro.texture();  // ask the coroutine to generate new data
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
