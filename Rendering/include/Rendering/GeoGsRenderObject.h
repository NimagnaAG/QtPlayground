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
class RENDERING_API GeoGsRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT

  friend class OpenGlWidget;

 public:
  // The source can deliver either RGB, RGBA or BGRA format
  enum class SourcePixelFormat { RGB, RGBA, BGRA };
  enum class TextureTarget { Target2D, TargetRectangle };
  static inline TextureTarget kDefaultTextureTarget = TextureTarget::Target2D;

  GeoGsRenderObject() = delete;
  GeoGsRenderObject(TextureTarget type);
  GeoGsRenderObject(TextureTarget type, const QString& location);
  // not copyable or movable
  GeoGsRenderObject(const GeoGsRenderObject& other) = delete;
  GeoGsRenderObject& operator=(const GeoGsRenderObject& other) = delete;
  GeoGsRenderObject(GeoGsRenderObject&&) = delete;
  GeoGsRenderObject& operator=(GeoGsRenderObject&&) = delete;
  virtual ~GeoGsRenderObject();
  // initializes the render object.
  virtual void initialize() override;

  void LoadSplatGs(const QString& location);
  void LoadAnimateGs(const QString& location) {}; 
 
  // draws the render object.
  virtual void draw() override; 
  void sort(const QMatrix4x4& viewProj);
  void resizeGL(int w, int h);
  bool isControl = false;
  virtual void keyPressEvent(QKeyEvent* event) override {
    SPDLOG_INFO("keyPressEvent");
      /* invertMatrix(viewMatrix); 
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
      */ 
  }; 
  
struct Vertex {
    QVector3D center;
    QVector3D scale;
    QVector4D rotation;
    QVector4D color;
  };

  std::vector<unsigned int> generateTexture();
  QString mGsLocation = "";
  int vertexCount = 0;
  int rowLength = 32;
  void initializeGL();    
  std::vector<QVector3D> m_positions;
  std::vector<QVector3D> m_scales;
  std::vector<QVector4D> m_rotations;
  std::vector<QVector4D> m_colors;
  QMatrix4x4 viewMatrix;  
  QOpenGLBuffer m_ebo; 
  bool isDataReady = false;
  int m_uViewLoc, m_uProjLoc, m_uFocalLoc, m_uViewportLoc;
  int viewportw = 1000, viewporth = 1000;

  struct SplatData {
    std::vector<QVector3D> positions;
    std::vector<QVector3D> scales;
    std::vector<QVector4D> rotations;
    std::vector<QVector4D> colors;
  };
  SplatData loadSplatFile(const QString& filePath) {
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
    /* std::ofstream outFile("texdata_output.txt");
    if (outFile.is_open()) {
      // Print positions
      outFile << "Positions:\n";
      for (size_t i = 0; i < result.positions.size(); ++i) {
        const auto& pos = result.positions[i];
        outFile << pos.x() << " " << pos.y() << " " << pos.z();
        if (i + 1 < result.positions.size()) outFile << " | ";
      }
      outFile << "\n";

      // Print scales
      outFile << "Scales:\n";
      for (size_t i = 0; i < result.scales.size(); ++i) {
        const auto& scale = result.scales[i];
        outFile << scale.x() << " " << scale.y() << " " << scale.z();
        if (i + 1 < result.scales.size()) outFile << " | ";
      }
      outFile << "\n";

      // Print colors
      outFile << "Colors:\n";
      for (size_t i = 0; i < result.colors.size(); ++i) {
        const auto& color = result.colors[i];
        outFile << color.x() << " " << color.y() << " " << color.z() << " " << color.w();
        if (i + 1 < result.colors.size()) outFile << " | ";
      }
      outFile << "\n";

      // Print rotations
      outFile << "Rotations:\n";
      for (size_t i = 0; i < result.rotations.size(); ++i) {
        const auto& rot = result.rotations[i];
        outFile << rot.x() << " " << rot.y() << " " << rot.z() << " " << rot.w();
        if (i + 1 < result.rotations.size()) outFile << " | ";
      }
      outFile << "\n";

      outFile.close();
      SPDLOG_INFO("texdata saved to texdata_output.txt");
    } else {
      SPDLOG_INFO("Failed to open file for writing texdata.");
    }
 */
    return result;
  }
  int focalWidth = 1500;
  int focalHeight = 1500;

   virtual void useExternalTexture(bool useExternal);
  // checks if the render object is visible on the screen

  // the texture's source size
  const QSize& textureSourceSize() const;
  const QSize& maskSourceSize() const;
  // the source's format (RGB, RGBA, BGRA) and type (static, streaming)
  SourcePixelFormat sourcePixelFormat() const;
  // the texture's and mask's real size
  const QSize& textureSize() const;
  const QSize& maskSize() const;

  // get the texture target
  const TextureTarget target() const { return mTextureTarget; }

  // the texture units for color and separate mask textures
  static const GLint colorTextureUnit() { return mColorTextureUnit; }
  static const GLint maskTextureUnit() { return mMaskTextureUnit; }
  // static helpers to translate target and pixel format to OpenGL and Qt constants
  static QOpenGLTexture::Target qGlTarget(TextureTarget target);
  static GLint glTarget(TextureTarget target);
  static QOpenGLTexture::PixelFormat qGlSourceFormat(SourcePixelFormat format);
  static GLint glSourceFormat(SourcePixelFormat format);

  bool hasSeparateMask() const;
  void enableSeparateMask(bool separateMaskEnabled, bool blurEnabled);

  // uploads the VertexData to GPU. Must be called after changing mVBD data
  void uploadVertexData();
  // change the mask size
  // update the mask texture data
  // set the position of a particular vertex. does not upload the data to the GPU -> call
  // uploadVertexData after changing the vertex data
 /// void setVertexPosition(int vertexId, int index, float value);   
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;

 private:
  // initialize the shader program
  void setupShaderProgram();
  // updates the texture coordinates if size has changed or flip flag has changed
  QMutex mAccessMutex;

  // helpers related to the texture target
  const QOpenGLTexture::Target qGlTarget() const;
  const GLint glTarget() const;
  // helpers related to the pixel format
  const QOpenGLTexture::PixelFormat qGlSourceFormat() const;
  const GLint glSourceFormat() const;
  static QImage::Format qImageFormatFromSourcePixelFormat(SourcePixelFormat format);
  static const std::map<SourcePixelFormat, QImage::Format> kSourcePixelFormatToQImageFormatMap;

  // The texture's type (2D or Rect)
  const TextureTarget mTextureTarget;
  // the Vertex Array Object holds all vertex relevant data
  QOpenGLVertexArrayObject mVAO;
  // the vertex buffer object
  QOpenGLBuffer mVBO;
  // the index buffer with the vertex indices for each triangle
  QOpenGLBuffer mIBO;
  // struct holding the data per vertex
  

  // the source format can be RGB, RGBA, or BGRA
  SourcePixelFormat mSourcePixelFormat = SourcePixelFormat::RGB;

  // the texture for static sources
  std::unique_ptr<QOpenGLTexture> mTexture;
  // the separate texture for the mask
  bool mSeparateMaskTextureEnabled = false;
  std::unique_ptr<QOpenGLTexture> mMaskTexture;

  // the texture source's width and height
  QSize mTextureSourceSize;
  // the source's mask width and height
  QSize mMaskSourceSize;
  // the texture width and height
  QSize mTextureSize;
  // the mask width and height
  QSize mMaskSize;

  // flag set if external texture is used.
  bool mUseExternalTexture = false;
  // render upside down
  bool mFlipVertically = false;
  // render output horizontally flipped
  bool mFlipHorizontally = false;
  // the transformation matrix location in the shader
  int mWorldTransformationShaderPosition = -1;
  // flag to enable or disable the blurring in the keyed_texture shader
  bool mCameraMaskBlurring = false;

  // texture units for color and mask texture
  static inline const GLint mColorTextureUnit = 2;
  static inline const GLint mMaskTextureUnit = 3;

  // As a performance optimization, texture sizes as multiples of four are considered to have better
  // performance. And on really old hardware, textures had to have a power of two size. It is
  // unlikely that this is still required since it is not a requirement since OpenGL 2.0.
  enum class TextureTarget2dRequirement { None, MultipleOfFour, PowerOfTwo };
  const TextureTarget2dRequirement mTextureTarget2dRequirement =
      TextureTarget2dRequirement::MultipleOfFour;
  // get next higher power of two number.
  static int nextPowerOfTwo(int input);
  // get next multiple of four number
  static int nextMultipleOfFour(int input);

  // void renderModel();
  //  std::vector<QOpenGLVertexArrayObject> mVAOs;
 // std::vector<std::unique_ptr<QOpenGLVertexArrayObject>> mVAOs;
   QMatrix4x4 mTransformMatrix;
  QMatrix4x4 mProjectionMatrix;
  GLuint mTextureID = 5;
  std::vector<GLuint> mTextureIDs; 
};

}  // namespace nimagna
