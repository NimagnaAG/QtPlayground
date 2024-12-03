#include "Rendering/pch.h"

#include "Rendering/GltfRenderObject.h"

#include <QtCore/QMutexLocker>
#include <QtCore/QRandomGenerator>
#include <QtCore/QThread>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGL/QOpenGLPixelTransferOptions>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Rendering/tiny_gltf.h>

namespace nimagna {

  const std::map<GltfRenderObject::SourcePixelFormat, QImage::Format>
    GltfRenderObject::kSourcePixelFormatToQImageFormatMap = {
        {GltfRenderObject::SourcePixelFormat::RGB, QImage::Format::Format_RGB888},
        {GltfRenderObject::SourcePixelFormat::RGBA,
         QImage::Format::Format_RGBA8888_Premultiplied},
        {GltfRenderObject::SourcePixelFormat::BGRA,
         QImage::Format::Format_RGBA8888_Premultiplied}};

GltfRenderObject::GltfRenderObject(TextureTarget type) : mTextureTarget(type) {
    enableSeparateMask(false, false);
    initialize();
  }

GltfRenderObject::GltfRenderObject(TextureTarget type, const QImage& texture)
    : GltfRenderObject(type) {
  enableSeparateMask(false, false);
  initialize();
 // setTextureData(texture);
 // setMaskTextureData(texture);
}

GltfRenderObject::~GltfRenderObject() {
  mVAO.destroy();
  mVBO.destroy();
  mIBO.destroy();
  mTexture.reset();
  mMaskTexture.reset();
  mShaderProgram.reset();

  mTransformMatrix.setToIdentity();
}

void GltfRenderObject::initialize() {
  initializeOpenGLFunctions();
  SPDLOG_DEBUG("Initializing GltfRenderObject");

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "D:\\3dassets\\avocado\\Avocado.gltf");
  if (!warn.empty()) {
    SPDLOG_WARN("GLTF Warning: {}", warn);
  }
  if (!err.empty()) {
    SPDLOG_ERROR("GLTF Error: {}", err);
  }
  if (!ret) {
    SPDLOG_ERROR("Failed to load GLTF model");
    return;
  }

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
  mVBO.bind();

  // Process the model (e.g., create OpenGL buffers)
  processModel(model);

  // load texture
  if (model.materials.empty() || model.textures.empty() || model.images.empty()) {
    SPDLOG_ERROR("No textures found in the model.");
    return;
  }

  const tinygltf::Texture& texture = model.textures[0];
  const tinygltf::Image& image = model.images[texture.source];

  glGenTextures(1, &mTextureID);
  glBindTexture(GL_TEXTURE_2D, mTextureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               image.image.data());
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  // Finally, build and compile the shader program
  setupShaderProgram();

  // Done
  RenderObject::initialize();
}

void GltfRenderObject::processModel(const tinygltf::Model& model) {

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      // Create and bind Vertex Array Object (VAO)
      auto vao = std::make_unique<QOpenGLVertexArrayObject>();
      if (!vao->create()) {
        SPDLOG_ERROR("Failed to create VertexArrayObject");
        continue;
      }
      vao->bind();

      // Create and bind Vertex Buffer Object (VBO)
      QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
      if (!vbo.create()) {
        SPDLOG_ERROR("Failed to create VertexBufferObject");
        continue;
      }
      vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

      // Assuming the primitive has positions
      const tinygltf::Accessor& posAccessor =
          model.accessors[primitive.attributes.find("POSITION")->second];
      const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
      const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

      // Copy and scale vertex positions
      std::vector<float> scaledPositions(posAccessor.count * 3);  // Assuming vec3 positions
      const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset]);
      float scaleFactor = 10.0f;  // Scale factor

      for (size_t i = 0; i < posAccessor.count; ++i) {
        scaledPositions[i * 3 + 0] = positions[i * 3 + 0] * scaleFactor;
        scaledPositions[i * 3 + 1] = positions[i * 3 + 1] * scaleFactor;
        scaledPositions[i * 3 + 2] = positions[i * 3 + 2] * scaleFactor;
      }

      vbo.bind();
      vbo.allocate(scaledPositions.data(),
                   static_cast<int>(scaledPositions.size() * sizeof(float)));

      // Set up vertex attribute pointers
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);

      // Create and bind Index Buffer Object (EBO) if it exists
      QOpenGLBuffer ebo(QOpenGLBuffer::IndexBuffer);
      int indexCount = 0;
      if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

        if (!ebo.create()) {
          SPDLOG_ERROR("Failed to create IndexBufferObject");
          continue;
        }
        ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        ebo.bind();
        ebo.allocate(&indexBuffer.data[indexView.byteOffset],
                     static_cast<int>(indexView.byteLength));

        indexCount = static_cast<int>(indexAccessor.count);  // Store the index count
      }

      // Unbind VAO
      vao->release();

      // Store the VAO and index count for rendering later
      mVAOs.push_back(std::move(vao));
      mIndexCounts.push_back(indexCount);
    }
  }
}

void GltfRenderObject::setupShaderProgram() {
  //////////////////////////////////////////////////////////////////////////
  // Create, initialize, and link
  //////////////////////////////////////////////////////////////////////////

  // create shader program
  mShaderProgram = std::make_unique<QOpenGLShaderProgram>();

  // read the vertex shader program from the resources
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                                        ":/resources/shaders/gltf.vert")) {
    SPDLOG_ERROR("Vertex shader error! {}", mShaderProgram->log().toStdString());
  }

  // read the fragment shader program from the resources
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/gltf.frag")) {
    SPDLOG_ERROR("Fragment shader error! {}", mShaderProgram->log().toStdString());
  }

  // link
  if (!mShaderProgram->link()) {
    SPDLOG_ERROR("Shader linker error! {}", mShaderProgram->log().toStdString());
  }

  // and bind
  if (!mShaderProgram->bind()) {
    SPDLOG_ERROR("Failed to bind shader program! {}", mShaderProgram->log().toStdString());
  }

  //////////////////////////////////////////////////////////////////////////
  // Define the format/assign the vertex data to the buffer indices
  //////////////////////////////////////////////////////////////////////////

  // Ensure VAO is bound
  mVAO.bind();
  // Ensure VBO is bound
  mVBO.bind();

  // Vertex data structure is as follows:
  // there are 4 vertices (see mVBD size)
  // - each vertex has: pos (3xfloat), normal (3xfloat), texCoords (2xfloat)
  const int positionCount = 3;
  const int normalCount = 3;
  const int texCoordsCount = 2;

  // layout location 0 - vec3 with coordinates
  mShaderProgram->enableAttributeArray(0);
  const int positionOffsetBytes = 0;
  mShaderProgram->setAttributeBuffer(0, GL_FLOAT, positionOffsetBytes, positionCount,
                                     sizeof(vertexData));

  // layout location 1 - vec3 with normals
  mShaderProgram->enableAttributeArray(1);
  const int normalOffsetBytes = positionCount * sizeof(float);
  mShaderProgram->setAttributeBuffer(1, GL_FLOAT, normalOffsetBytes, normalCount,
                                     sizeof(vertexData));

  // layout location 2 - vec2 with texture coordinates
  mShaderProgram->enableAttributeArray(2);
  const int texCoordsOffsetBytes = normalOffsetBytes + normalCount * sizeof(float);
  mShaderProgram->setAttributeBuffer(2, GL_FLOAT, texCoordsOffsetBytes, texCoordsCount,
                                     sizeof(vertexData));

   mVAO.release();
}


void GltfRenderObject::draw() {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }
  mShaderProgram->bind();

  
  // Update the rotation angle
  mRotationAngle += 1.0f;  // Adjust the speed of rotation as needed

  // Create the model matrix with rotation
  QMatrix4x4 modelMatrix;
  modelMatrix.rotate(mRotationAngle, QVector3D(1.0f, 1.0f, 0.0f));  // Rotate around the Y-axis

  // Set the model, view, and projection matrices
  QMatrix4x4 viewMatrix;        // Set this to your view matrix
  QMatrix4x4 projectionMatrix;  // Set this to your projection matrix

  mShaderProgram->setUniformValue("model", modelMatrix);
  mShaderProgram->setUniformValue("view", viewMatrix);
  mShaderProgram->setUniformValue("projection", projectionMatrix);


  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, mTextureID);
  mShaderProgram->setUniformValue("texture_diffuse1", 0);

  for (size_t i = 0; i < mVAOs.size(); ++i) {
    mVAOs[i]->bind();
    glDrawElements(GL_TRIANGLES, mIndexCounts[i], GL_UNSIGNED_SHORT, 0);
    mVAOs[i]->release();
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  mShaderProgram->release();
}

bool GltfRenderObject::hasSeparateMask() const {
  return mSeparateMaskTextureEnabled;
}

void GltfRenderObject::enableSeparateMask(bool separateMaskEnabled, bool blurEnabled) {
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

/* Red simple shader
void GltfRenderObject::setupShaderProgram() {
  //////////////////////////////////////////////////////////////////////////
  // Create, initialize, and link
  //////////////////////////////////////////////////////////////////////////

  // create shader program
  mShaderProgram = std::make_unique<QOpenGLShaderProgram>();

  // read the vertex shader program from the resources
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex, mVertexShaderFile)) {
    SPDLOG_ERROR("Vertex shader error! {}", mShaderProgram->log().toStdString());
  }

  // read the simplified fragment shader program
  if (!mShaderProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/simple_red.frag")) {
    SPDLOG_ERROR("Fragment shader error! {}", mShaderProgram->log().toStdString());
  }

  // link
  if (!mShaderProgram->link()) {
    SPDLOG_ERROR("Shader linker error! {}", mShaderProgram->log().toStdString());
  }

  // and bind
  if (!mShaderProgram->bind()) {
    SPDLOG_ERROR("Failed to bind shader program! {}", mShaderProgram->log().toStdString());
  }

  //////////////////////////////////////////////////////////////////////////
  // Set values
  //////////////////////////////////////////////////////////////////////////

  // set projection matrix to identity
  mWorldTransformationShaderPosition = mShaderProgram->uniformLocation("worldToView");
  if (mWorldTransformationShaderPosition == -1) {
    SPDLOG_ERROR("Invalid world transformation ID: {}", mShaderProgram->log().toStdString());
  }
  QMatrix4x4 projectionMatrix;
  projectionMatrix.setToIdentity();
  mShaderProgram->setUniformValue(mWorldTransformationShaderPosition, projectionMatrix);

  //////////////////////////////////////////////////////////////////////////
  // Define the format/assign the vertex data to the buffer indices
  //////////////////////////////////////////////////////////////////////////

  // Vertex data structure is as follows:
  // there are 4 vertices (see mVBD size)
  // - each vertex has: pos (3xfloat)
  const int positionCount = 3;

  // layout location 0 - vec3 with coordinates
  mShaderProgram->enableAttributeArray(0);
  const int positionOffsetBytes = 0;
  mShaderProgram->setAttributeBuffer(0, GL_FLOAT, positionOffsetBytes, positionCount,
                                     sizeof(vertexData));
}
*/
/* Working simple
* 
void GltfRenderObject::draw() {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }
  mShaderProgram->bind();

   // Pass the transformation matrix to the shader
  GLuint transformLoc = mShaderProgram->uniformLocation("worldToView");
  if (transformLoc == -1) {
    SPDLOG_ERROR("Failed to get uniform location for transform.");
    return;
  }
  mShaderProgram->setUniformValue(transformLoc, mTransformMatrix);

  for (size_t i = 0; i < mVAOs.size(); ++i) {
    mVAOs[i]->bind();
    glDrawElements(GL_TRIANGLES, mIndexCounts[i], GL_UNSIGNED_SHORT, 0);
    mVAOs[i]->release();
  }

  mShaderProgram->release();
}

void GltfRenderObject::draw() {
  if (isEmpty()) {
    return;
  }
  if (!mTexture) {
    SPDLOG_WARN("Draw static source without a texture");
    return;
  }
  // thread critical section
  QMutexLocker locker(&mAccessMutex);
  // use the shader program
  if (!mShaderProgram->bind()) {
    SPDLOG_ERROR("Failed to bind texture program");
  }

  // set projection matrix
  const QMatrix4x4 mvp = mViewProjectionMatrix * getModelMatrix();
  mShaderProgram->setUniformValue(mWorldTransformationShaderPosition, mvp);

  // set alpha transparency value [0.0, 1.0]
  mShaderProgram->setUniformValue("alphaTransparency", static_cast<GLfloat>(alpha()));
  // need to swap R and B channel for BGRA source
  mShaderProgram->setUniformValue(
      "swapRGB",
      static_cast<GLboolean>(sourcePixelFormat() == SourcePixelFormat::BGRA));

  // use separate mask texture?
  mShaderProgram->setUniformValue("useMaskTexture", static_cast<int>(hasSeparateMask()));

  // bind the vertex array object (which uses the vertex buffer object)
  mVAO.bind();
  if (!mUseExternalTexture) {
    // bind the textures only if no external texture is used
    // use color texture unit
    glActiveTexture(GL_TEXTURE0 + mColorTextureUnit);
    // static texture
    mTexture->bind();
    if (hasSeparateMask()) {
      // enable the keying texture on mask texture unit
      glActiveTexture(GL_TEXTURE0 + mMaskTextureUnit);
      if (mMaskTexture != nullptr) {
        // static mask texture
        mMaskTexture->bind();
      } 
      glActiveTexture(GL_TEXTURE0 + mColorTextureUnit);
    }
  }
  // draw the two triangles
 // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

  for (size_t i = 0; i < mVAOs.size(); ++i) {
    mVAOs[i]->bind();
    glDrawElements(GL_TRIANGLES, mIndexCounts[i], GL_UNSIGNED_SHORT, 0);
    mVAOs[i]->release();
  }

  // release (for completeness)
  if (!mUseExternalTexture) {
    // static texture
    mTexture->release();
    if (hasSeparateMask()) {
      glActiveTexture(GL_TEXTURE0 + mMaskTextureUnit);
      if (mMaskTexture != nullptr) {
        // static mask texture
        mMaskTexture->release();
      } 
      glActiveTexture(GL_TEXTURE0 + mColorTextureUnit);
    }
  }
  mVAO.release();
  mShaderProgram->release();
}
*/

void GltfRenderObject::useExternalTexture(bool useExternal) {
  SPDLOG_DEBUG("Using external texture for rendering");
  mUseExternalTexture = useExternal;
}

bool GltfRenderObject::isEmpty() const {
  return mTextureSize.isEmpty();
}

const QSize& GltfRenderObject::textureSourceSize() const {
  return mTextureSourceSize;
}

const QSize& GltfRenderObject::maskSourceSize() const {
  return mMaskSourceSize;
}

const QSize& GltfRenderObject::textureSize() const {
  return mTextureSize;
}

const QSize& GltfRenderObject::maskSize() const {
  return mMaskSize;
}

GltfRenderObject::SourcePixelFormat GltfRenderObject::sourcePixelFormat() const {
  return mSourcePixelFormat;
}

bool GltfRenderObject::isVisible() const {
  // find the limits of the object, for 2D is enough to decide whether it is visible or not
  const QMatrix4x4 mvp = mViewProjectionMatrix * getModelMatrix();
  QVector3D firstVertexPosition(mVBD[0].position[0], mVBD[0].position[1], mVBD[0].position[2]);
  QVector3D firstVertexScreenPosition = mvp.map(firstVertexPosition);
  float minX = firstVertexScreenPosition.x();
  float maxX = firstVertexScreenPosition.x();
  float minY = firstVertexScreenPosition.y();
  float maxY = firstVertexScreenPosition.y();
  float minZ = firstVertexScreenPosition.z();
  float maxZ = firstVertexScreenPosition.z();
  for (int v = 1; v < 4; ++v) {
    QVector3D vertexPosition(mVBD[v].position[0], mVBD[v].position[1], mVBD[v].position[2]);
    QVector3D vertexScreenPosition = mvp.map(vertexPosition);
    minX = (vertexScreenPosition.x() < minX) ? vertexScreenPosition.x() : minX;
    maxX = (vertexScreenPosition.x() > maxX) ? vertexScreenPosition.x() : maxX;
    minY = (vertexScreenPosition.y() < minY) ? vertexScreenPosition.y() : minY;
    maxY = (vertexScreenPosition.y() > maxY) ? vertexScreenPosition.y() : maxY;
    minZ = (vertexScreenPosition.z() < minZ) ? vertexScreenPosition.z() : minZ;
    maxZ = (vertexScreenPosition.z() > maxZ) ? vertexScreenPosition.z() : maxZ;
  }
  // check if any of the limits found are within a cube of [-1,-1,-1] to [1,1,1]
  // works for both the 2D orthographic projection and the 3D perspective projections
  return maxX > -1.0f && minX < 1.0f && maxY > -1.0f && minY < 1.0f && maxZ > -1.0f && minZ < 1.0f;
}

const QOpenGLTexture::Target GltfRenderObject::qGlTarget() const {
  return qGlTarget(mTextureTarget);
}

QOpenGLTexture::Target GltfRenderObject::qGlTarget(TextureTarget target) {
  return target == TextureTarget::Target2D ? QOpenGLTexture::Target2D
                                           : QOpenGLTexture::TargetRectangle;
}

const GLint GltfRenderObject::glTarget() const {
  return glTarget(mTextureTarget);
}

GLint GltfRenderObject::glTarget(TextureTarget target) {
  return target == GltfRenderObject::TextureTarget::Target2D ? GL_TEXTURE_2D
                                                                : GL_TEXTURE_RECTANGLE;
}

const QOpenGLTexture::PixelFormat GltfRenderObject::qGlSourceFormat() const {
  return qGlSourceFormat(mSourcePixelFormat);
}

QOpenGLTexture::PixelFormat GltfRenderObject::qGlSourceFormat(
    SourcePixelFormat format) {
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

GLint GltfRenderObject::glSourceFormat(SourcePixelFormat format) {
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

const GLint GltfRenderObject::glSourceFormat() const {
  return glSourceFormat(mSourcePixelFormat);
}

QImage::Format GltfRenderObject::qImageFormatFromSourcePixelFormat(SourcePixelFormat format) {
  return kSourcePixelFormatToQImageFormatMap.at(format);
}

void GltfRenderObject::uploadVertexData() {
  // upload to GPU
  mVBO.bind();
  mVBO.allocate(mVBD.data(), static_cast<int>(mVBD.size() * sizeof(vertexData)));
}

void GltfRenderObject::changeTextureSizeAndFormat(QSize size,
                                                     SourcePixelFormat srcPixelFormat) {
  // thread critical section
  QMutexLocker locker(&mAccessMutex);
  if (mTextureSourceSize == size && srcPixelFormat == mSourcePixelFormat ) {
    // all the same
    return;
  }
  // update source size
  mTextureSourceSize = size;

  // calculate new texture size (and stop if the same)
  auto newTextureSize = size;
  if (mTextureTarget == TextureTarget::Target2D) {
    switch (mTextureTarget2dRequirement) {
      case TextureTarget2dRequirement::MultipleOfFour:
        newTextureSize = QSize(nextMultipleOfFour(size.width()), nextMultipleOfFour(size.height()));
        break;
      case TextureTarget2dRequirement::PowerOfTwo:
        newTextureSize = QSize(nextPowerOfTwo(size.width()), nextPowerOfTwo(size.height()));
        break;
      case TextureTarget2dRequirement::None:
      default:
        break;
    }
  }
  if (newTextureSize == mTextureSize && srcPixelFormat == mSourcePixelFormat) {
    // same texture size -> no change needed
    return;
  }

  mTextureSize = newTextureSize;
  mSourcePixelFormat = srcPixelFormat;

  if (isEmpty()) {
    if (mTexture) mTexture->destroy();
    mTexture.reset(nullptr);
  } else {
    // create new texture and allocate memory on GPU
    mTexture = std::make_unique<QOpenGLTexture>(qGlTarget());
    if (!mTexture->create()) {
      SPDLOG_ERROR("Unable to create texture");
      assert(false);
    }
    mTexture->setSize(mTextureSize.width(), mTextureSize.height());
    mTexture->setFormat(
        srcPixelFormat == SourcePixelFormat::RGB
            ? QOpenGLTexture::TextureFormat::RGB8_UNorm     // three channels, each 8 bits
            : QOpenGLTexture::TextureFormat::RGBA8_UNorm);  // four channels, each 8 bits
    mTexture->allocateStorage();
    mTexture->setMinificationFilter(QOpenGLTexture::Linear);
    mTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    mTexture->setBorderColor(Qt::transparent);
  }
  updateTextureCoordinates();
}

void GltfRenderObject::changeMaskSize(QSize size) {
  // thread critical section
  QMutexLocker locker(&mAccessMutex);
  if (size == mMaskSize) {
    return;
  }
  const auto sourceWidth = size.width();
  const auto sourceHeight = size.height();
  SPDLOG_DEBUG("Change mask size to {}x{}", sourceWidth, sourceHeight);
  if (!hasSeparateMask()) {
    SPDLOG_ERROR("Set mask size without separate mask enabled on {}", getDisplayName());
    return;
  }
  assert(hasSeparateMask());

  // update mask source size
  mMaskSourceSize = size;

  // calculate new texture size (and stop if the same)
  auto newMaskTextureSize = size;
  if (mTextureTarget == TextureTarget::Target2D) {
    switch (mTextureTarget2dRequirement) {
      case TextureTarget2dRequirement::MultipleOfFour:
        newMaskTextureSize =
            QSize(nextMultipleOfFour(sourceWidth), nextMultipleOfFour(sourceHeight));
        break;
      case TextureTarget2dRequirement::PowerOfTwo:
        newMaskTextureSize = QSize(nextPowerOfTwo(sourceWidth), nextPowerOfTwo(sourceHeight));
        break;
      case TextureTarget2dRequirement::None:
      default:
        break;
    }
  }
  if (newMaskTextureSize == mMaskSize) {
    // same texture size
    return;
  }
  mMaskSize = newMaskTextureSize;
  const auto maskWidth = mMaskSize.width();
  const auto maskHeight = mMaskSize.height();

  mMaskTexture = std::make_unique<QOpenGLTexture>(qGlTarget());
  if (!mMaskTexture->create()) {
    SPDLOG_ERROR("Unable to create keying texture");
    assert(false);
  }
  mMaskTexture->setSize(maskWidth, maskHeight);
  mMaskTexture->setFormat(QOpenGLTexture::R8_UNorm);  // single channel, 8 bits
  mMaskTexture->allocateStorage();
  mMaskTexture->setMinificationFilter(QOpenGLTexture::Linear);
  mMaskTexture->setMagnificationFilter(QOpenGLTexture::Linear);
  mMaskTexture->setBorderColor(Qt::transparent);
  mMaskTexture->setWrapMode(QOpenGLTexture::WrapMode::ClampToBorder);

  updateMaskTextureCoordinates();
}

void GltfRenderObject::setFlipVertically(bool flipVertically) {
  mFlipVertically = flipVertically;
  updateTextureCoordinates();
  updateMaskTextureCoordinates();
}

void GltfRenderObject::setFlipHorizontally(bool flipHorizontally) {
  mFlipHorizontally = flipHorizontally;
  updateTextureCoordinates();
  updateMaskTextureCoordinates();
}

void GltfRenderObject::setTextureData(const QImage& image) {
  if (image.isNull()) {
    SPDLOG_WARN("Set texture data received a NULL image!");
    return;
  }

  // set the texture size (if necessary)
  const auto srcPixelFormat = sourcePixelFormat();
  if (srcPixelFormat == SourcePixelFormat::BGRA) {
    SPDLOG_ERROR("BGRA not supported with QImage texture data!");
    assert(false);
    return;
  }
  const auto imageFormat = image.format();
  changeTextureSizeAndFormat(image.size(), srcPixelFormat);
  {
    // thread critical section
    QMutexLocker locker(&mAccessMutex);
    if (mTexture->isCreated() && mTexture->isStorageAllocated()) {
      mTexture->bind();
      if (imageFormat != qImageFormatFromSourcePixelFormat(srcPixelFormat)) {
        // needs conversion
        QImage texture =
            image.convertToFormat(qImageFormatFromSourcePixelFormat(srcPixelFormat));
        mTexture->setData(0, 0, 0, mTextureSourceSize.width(), mTextureSourceSize.height(), 0, 0,
                          qGlSourceFormat(), QOpenGLTexture::UInt8,
                          static_cast<const void*>(texture.bits()));
      } else {
        // use directly
        mTexture->setData(0, 0, 0, mTextureSourceSize.width(), mTextureSourceSize.height(), 0, 0,
                          qGlSourceFormat(), QOpenGLTexture::UInt8,
                          static_cast<const void*>(image.bits()));
      }
    }
  }
}

void GltfRenderObject::setMaskTextureData(const QImage& image) {
  if (!mSeparateMaskTextureEnabled) return;
  if (image.isNull()) {
    SPDLOG_WARN("Set key texture data received a NULL image!");
    return;
  }

  // set the key texture size (if necessary)
  changeMaskSize(image.size());
  {
    // thread critical section
    QMutexLocker locker(&mAccessMutex);
    QImage key = image;
    if (key.format() != QImage::Format_Alpha8) {
      key = key.convertToFormat(QImage::Format_Alpha8);
    }
    if (mMaskTexture->isCreated() && mMaskTexture->isStorageAllocated()) {
      mMaskTexture->setData(0, 0, 0, mMaskSourceSize.width(), mMaskSourceSize.height(), 0, 0,
                            QOpenGLTexture::Red, QOpenGLTexture::UInt8,
                            static_cast<const void*>(key.bits()));
    }
  }
}

void GltfRenderObject::setVertexPosition(int vertexId, int index, float value) {
  if (vertexId >= mVBD.size() || index >= 3) return;
  mVBD[vertexId].position[index] = value;
}

void GltfRenderObject::updateTextureCoordinates() {
  // returns left, top, right, bottom, width, height (the latter two for convenience)
  auto vertexPositions = textureVertexPositions(mTextureSourceSize);

  // vertex data array
  const float vertices[] = {
      vertexPositions[2],
      vertexPositions[1],
      0.0f,  // top right
      vertexPositions[2],
      vertexPositions[3],
      0.0f,  // bottom right
      vertexPositions[0],
      vertexPositions[3],
      0.0f,  // bottom left
      vertexPositions[0],
      vertexPositions[1],
      0.0f  // top left
  };

  // the corresponding texture coordinates
  // (initialized for rectangular target with coordinates in [0,w]x[0,h])
  auto widthValue = static_cast<float>(mTextureSourceSize.width());
  auto heightValue = static_cast<float>(mTextureSourceSize.height());
  if (mTextureTarget == TextureTarget::Target2D) {
    // for target 2D, the texture coordinates are normalized in [0.0,1.0]
    widthValue =
        (mTextureSize.width() > 0) ? widthValue / static_cast<float>(mTextureSize.width()) : 1.f;
    heightValue =
        (mTextureSize.height() > 0) ? heightValue / static_cast<float>(mTextureSize.height()) : 1.f;
  }

  float textureCoords[8] = {0.f};
  if (mFlipVertically == false && mFlipHorizontally == false) {
    textureCoords[0] = widthValue;  // top right
    textureCoords[1] = heightValue;
    textureCoords[2] = widthValue;  // bottom right
    textureCoords[3] = 0.f;
    textureCoords[4] = 0.f;  // bottom left
    textureCoords[5] = 0.f;
    textureCoords[6] = 0.f;  // top left
    textureCoords[7] = heightValue;
  } else if (mFlipVertically == true && mFlipHorizontally == false) {
    textureCoords[0] = widthValue;  // top right
    textureCoords[1] = 0.f;
    textureCoords[2] = widthValue;  // bottom right
    textureCoords[3] = heightValue;
    textureCoords[4] = 0.f;  // bottom left
    textureCoords[5] = heightValue;
    textureCoords[6] = 0.f;  // top left
    textureCoords[7] = 0.f;
  } else if (mFlipVertically == false && mFlipHorizontally == true) {
    textureCoords[0] = 0.f;  // top right
    textureCoords[1] = heightValue;
    textureCoords[2] = 0.f;  // bottom right
    textureCoords[3] = 0.f;
    textureCoords[4] = widthValue;  // bottom left
    textureCoords[5] = 0.f;
    textureCoords[6] = widthValue;  // top left
    textureCoords[7] = heightValue;
  } else {
    // vertical: true, horizontal: true
    textureCoords[0] = 0.f;
    textureCoords[1] = 0.f;
    textureCoords[2] = 0.f;
    textureCoords[3] = heightValue;
    textureCoords[4] = widthValue;
    textureCoords[5] = heightValue;
    textureCoords[6] = widthValue;
    textureCoords[7] = 0.f;
  }
  // update VBD
  for (int v = 0.f; v < 4; ++v) {
    for (int p = 0.f; p < 3; ++p) {
      mVBD[v].position[p] = vertices[v * 3 + p];
    }
    mVBD[v].texture[0] = textureCoords[2 * v];
    mVBD[v].texture[1] = textureCoords[2 * v + 1];
  }
  uploadVertexData();
}

void GltfRenderObject::updateMaskTextureCoordinates() {
  auto widthValue = static_cast<float>(mMaskSourceSize.width());
  auto heightValue = static_cast<float>(mMaskSourceSize.height());
  if (mTextureTarget == TextureTarget::Target2D) {
    widthValue = (mMaskSize.width() > 0) ? widthValue / static_cast<float>(mMaskSize.width()) : 1.f;
    heightValue =
        (mMaskSize.height() > 0) ? heightValue / static_cast<float>(mMaskSize.height()) : 1.f;
  }

  // Note: Vertex positions are set only for the texture itself, not the mask
  float maskTextureCoords[8] = {0.f};
  if (mFlipVertically == false && mFlipHorizontally == false) {
    maskTextureCoords[0] = widthValue;  // top right
    maskTextureCoords[1] = heightValue;
    maskTextureCoords[2] = widthValue;  // bottom right
    maskTextureCoords[3] = 0.f;
    maskTextureCoords[4] = 0.f;  // bottom left
    maskTextureCoords[5] = 0.f;
    maskTextureCoords[6] = 0.f;  // top left
    maskTextureCoords[7] = heightValue;
  } else if (mFlipVertically == true && mFlipHorizontally == false) {
    maskTextureCoords[0] = widthValue;  // top right
    maskTextureCoords[1] = 0.f;
    maskTextureCoords[2] = widthValue;  // bottom right
    maskTextureCoords[3] = heightValue;
    maskTextureCoords[4] = 0.f;  // bottom left
    maskTextureCoords[5] = heightValue;
    maskTextureCoords[6] = 0.f;  // top left
    maskTextureCoords[7] = 0.f;
  } else if (mFlipVertically == false && mFlipHorizontally == true) {
    maskTextureCoords[0] = 0.f;  // top right
    maskTextureCoords[1] = heightValue;
    maskTextureCoords[2] = 0.f;  // bottom right
    maskTextureCoords[3] = 0.f;
    maskTextureCoords[4] = widthValue;  // bottom left
    maskTextureCoords[5] = 0.f;
    maskTextureCoords[6] = widthValue;  // top left
    maskTextureCoords[7] = heightValue;
  } else {
    // vertical: true, horizontal: true
    maskTextureCoords[0] = 0.f;
    maskTextureCoords[1] = 0.f;
    maskTextureCoords[2] = 0.f;
    maskTextureCoords[3] = heightValue;
    maskTextureCoords[4] = widthValue;
    maskTextureCoords[5] = heightValue;
    maskTextureCoords[6] = widthValue;
    maskTextureCoords[7] = 0.f;
  }
  // update VBD
  for (int v = 0; v < 4; ++v) {
    mVBD[v].maskTexture[0] = maskTextureCoords[2 * v];
    mVBD[v].maskTexture[1] = maskTextureCoords[2 * v + 1];
  }
  uploadVertexData();
}

int GltfRenderObject::nextPowerOfTwo(int input) {
  // Taken from
  // https://stackoverflow.com/questions/466204/rounding-up-to-next-power-of-2
  input--;
  input |= input >> 1;
  input |= input >> 2;
  input |= input >> 4;
  input |= input >> 8;
  input |= input >> 16;
  input++;
  return input;
}

int GltfRenderObject::nextMultipleOfFour(int input) {
  // from https://stackoverflow.com/questions/2022179/c-quick-calculation-of-next-multiple-of-4
  return (input + 3) & ~0x03;
}


std::array<float, 6> GltfRenderObject::textureVertexPositions(const QSize& textureSize) {
  // Note: This code used to be in GltfRenderObject but has moved here since it is also used in
  // HeadPoseEvaluator

  // Calculate the vertex positions (corners of the texture in unit space)
  // full screen is default
  float left = -1.0f;
  float right = 1.0f;
  float bottom = -1.0f;
  float top = 1.0f;

  // Preserve texture aspect ratio
  const float textureAspectRatio =
      static_cast<float>(textureSize.width()) / static_cast<float>(textureSize.height());

  // Attention: The full output corresponds is usually a 16:9 output
  const auto outputSize = QSize(1080, 720);
  // In order to maintain the texture's aspect ratio, we have to calculate vertex positions
  // respecting both the texture aspect ratio _and_ the output aspect ratio.
  // E.g. for 16:9 textures, the vertices result in the full screen (-1/1). Thus, we multiply the
  // texture aspect ratio with the inverse of the output aspect ratio:
  if (const auto vertexAspectRatio = textureAspectRatio * static_cast<float>(outputSize.height()) /
                                     static_cast<float>(outputSize.width());
      vertexAspectRatio > 1.f) {
    // shrink top/bottom
    const auto inverseAspectRatio = 1.f / vertexAspectRatio;
    top = inverseAspectRatio;
    bottom = -inverseAspectRatio;
  } else if (vertexAspectRatio < 1.f) {
    // shrink left/right
    right = vertexAspectRatio;
    left = -vertexAspectRatio;
  }
  // returns left, top, right, bottom, width, height (the latter two for convenience)
  // Note that this coordinate system has x from left to right and y from bottom to top!
  return {left, top, right, bottom, right - left, top - bottom};
}

}  // namespace nimagna
