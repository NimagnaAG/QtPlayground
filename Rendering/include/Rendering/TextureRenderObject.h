#pragma once

#include <QtCore/QMutex>
#include <QtCore/QSize>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <vector>

#include "RenderObject.h"

namespace nimagna {

// a render object rendering a textured rectangle, potentially with a separate mask/key/alpha
// texture
class RENDERING_API TextureRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT

  friend class OpenGlWidget;

 public:
  // The source can deliver either RGB, RGBA or BGRA format
  enum class SourcePixelFormat { RGB, RGBA, BGRA };
  enum class TextureTarget { Target2D, TargetRectangle };
  static inline TextureTarget kDefaultTextureTarget = TextureTarget::Target2D;

  TextureRenderObject() = delete;
  TextureRenderObject(TextureTarget type);
  TextureRenderObject(TextureTarget type, const QImage& texture);
  // not copyable or movable
  TextureRenderObject(const TextureRenderObject& other) = delete;
  TextureRenderObject& operator=(const TextureRenderObject& other) = delete;
  TextureRenderObject(TextureRenderObject&&) = delete;
  TextureRenderObject& operator=(TextureRenderObject&&) = delete;
  virtual ~TextureRenderObject();

  // initializes the render object.
  virtual void initialize() override;

  // draws the render object.
  virtual void draw() override;

  // get the source's texture and mask size
  bool isEmpty() const;
  // normally, the TextureRenderObject renders its own texture. If this flag is set, the TRO assumes
  // an external texture is bound during rendering and does not bind its own texture(s).
  virtual void useExternalTexture(bool useExternal);
  // checks if the render object is visible on the screen
  bool isVisible() const;

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
  // change the texture size and format
  virtual void changeTextureSizeAndFormat(QSize size, SourcePixelFormat pixelFormat);
  // change the mask size
  virtual void changeMaskSize(QSize size);

  // flip vertically
  void setFlipVertically(bool flipVertically);
  // mirror horizontally
  void setFlipHorizontally(bool flipHorizontally);
  // update the texture data
  void setTextureData(const QImage& image);
  // update the mask texture data
  void setMaskTextureData(const QImage& image);
  // set the position of a particular vertex. does not upload the data to the GPU -> call
  // uploadVertexData after changing the vertex data
  void setVertexPosition(int vertexId, int index, float value);

 protected:
  // the vertex shader code
  static const inline QString mVertexShaderFile = ":/resources/shaders/texture.vert";
  // the fragment shader code
  static const inline std::map<TextureTarget, QString> mFragmentShaderFile = {
      {TextureTarget::Target2D, ":/resources/shaders/texture_2d.frag"},
      {TextureTarget::TargetRectangle, ":/resources/shaders/texture_rectangle.frag"}};
  // the shaders
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;


 private:
  // initialize the shader program
  void setupShaderProgram();
  // updates the texture coordinates if size has changed or flip flag has changed
  void updateTextureCoordinates();
  // updates the mask's texture coordinates if size has changed or flip flag has changed
  void updateMaskTextureCoordinates();

  QMutex mAccessMutex;

  // helpers related to the texture target
  const QOpenGLTexture::Target qGlTarget() const;
  const GLint glTarget() const;
  // helpers related to the pixel format
  const QOpenGLTexture::PixelFormat qGlSourceFormat() const;
  const GLint glSourceFormat() const;
  static QImage::Format qImageFormatFromSourcePixelFormat(SourcePixelFormat format);
  static const std::map<SourcePixelFormat, QImage::Format>
      kSourcePixelFormatToQImageFormatMap;

  // The texture's type (2D or Rect)
  const TextureTarget mTextureTarget;
  // the Vertex Array Object holds all vertex relevant data
  QOpenGLVertexArrayObject mVAO;
  // the vertex buffer object
  QOpenGLBuffer mVBO;
  // the index buffer with the vertex indices for each triangle
  QOpenGLBuffer mIBO;
  // struct holding the data per vertex
  struct vertexData {
    float position[3];
    float texture[2];
    float maskTexture[2];
  };
  // the vertex buffer data that is uploaded to the vertex buffer object
  std::vector<vertexData> mVBD;

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

    // calculates the vertex positions of a textured rectangle
  // returns left, top, right, bottom, width, height (the latter two for convenience)
  // normally, this is [-1,-1,1,1,2,2] but if the texture width/height are not 16/9, the vertices
  // must be adapted
  // Note that this coordinate system has x from left to right and y from bottom to top
  static std::array<float, 6> textureVertexPositions(const QSize& textureSize);
};

}  // namespace nimagna
