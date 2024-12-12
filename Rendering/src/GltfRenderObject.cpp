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
#include<cmath>

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
  }

GltfRenderObject::GltfRenderObject(TextureTarget type, const QString& location)
    : GltfRenderObject(type) {
  //enableSeparateMask(false, false);
    mGltfLocation = location;
    initialize();
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

  // Set up the projection matrix
  const float aspectRatio = 1.0f;
  const float nearPlane = 0.01f;
  const float farPlane = 1000.f;
 
  mProjectionMatrix.perspective(90, aspectRatio, nearPlane, farPlane);
  //mProjectionMatrix.ortho(-1.0f, 1.0f, -1.0f, 1.0f, nearPlane, farPlane);
  SPDLOG_INFO("Projection matrix setup");

  SPDLOG_INFO("Initializing GltfRenderObject with location: " + mGltfLocation.toStdString());

  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, mGltfLocation.toStdString());
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

  loadTextures(model);

  // Finally, build and compile the shader program
  setupShaderProgram();

  // Done
  RenderObject::initialize();
}

void GltfRenderObject::loadTextures(const tinygltf::Model& model) {
  for (const auto& material : model.materials) {
      if (material.values.find("baseColorTexture") != material.values.end()) {
        int textureIndex = material.values.at("baseColorTexture").TextureIndex();
        const tinygltf::Texture& texture = model.textures[textureIndex];
        const tinygltf::Image& image = model.images[texture.source];

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, image.image.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        mTextureIDs.push_back(textureID);
      }
  }
  glBindTexture(GL_TEXTURE_2D, 0);
}

struct Vertex {
  QVector3D position;
  QVector3D normal;
  QVector2D texCoords;
};

std::vector<Vertex> vertices;
std::vector<GLuint> indices;


void GltfRenderObject::processModel(const tinygltf::Model& model) {
  float scaleFactor = 1.0f;  // Scale factor

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

        // Extract positions, normals, and texCoords
        const tinygltf::Accessor& posAccessor =
            model.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
        const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];
        const float* positions = reinterpret_cast<const float*>(
            &posBuffer.data[posView.byteOffset + posAccessor.byteOffset]);

        const float* normals = nullptr;
        auto normalIt = primitive.attributes.find("NORMAL");  // Some models might not have normals
        if (normalIt != primitive.attributes.end()) {
          const tinygltf::Accessor& normAccessor =
              model.accessors[primitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
          const tinygltf::Buffer& normBuffer = model.buffers[normView.buffer];
          normals = reinterpret_cast<const float*>(
              &normBuffer.data[normView.byteOffset + normAccessor.byteOffset]);
        }

        const float* texCoords = nullptr;
        auto texIt = primitive.attributes.find("TEXCOORD_0");
        if (texIt != primitive.attributes.end()) {
          const tinygltf::Accessor& texAccessor = model.accessors[texIt->second];
          const tinygltf::BufferView& texView = model.bufferViews[texAccessor.bufferView];
          const tinygltf::Buffer& texBuffer = model.buffers[texView.buffer];
          texCoords = reinterpret_cast<const float*>(
              &texBuffer.data[texView.byteOffset + texAccessor.byteOffset]);
        }

        std::vector<Vertex> vertices;
        for (size_t i = 0; i < posAccessor.count; ++i) {
          Vertex vertex;
          vertex.position =
              QVector3D(positions[i * 3] * scaleFactor, positions[i * 3 + 1] * scaleFactor,
                        positions[i * 3 + 2] * scaleFactor);

          if (normals) {
            vertex.normal = QVector3D(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
          } else {
            vertex.normal = QVector3D(0.0f, 0.0f, 0.0f);  // Default normal if not present
          }

          if (texCoords) {
            vertex.texCoords = QVector2D(texCoords[i * 2], texCoords[i * 2 + 1]);
          } else {
            vertex.texCoords = QVector2D(0.0f, 0.0f);  // Default texCoords if not present
          }

          vertices.push_back(vertex);
        }

        vbo.bind();
        vbo.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(Vertex)));

        // Set up vertex attribute pointers
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, texCoords));
        glEnableVertexAttribArray(2);

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
          ebo.allocate(&indexBuffer.data[indexView.byteOffset + indexAccessor.byteOffset],
                       static_cast<int>(indexAccessor.count * sizeof(unsigned short)));

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
   //glClear(GL_DEPTH_BUFFER_BIT);
   //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   mShaderProgram->bind();

   // Update the rotation angle
   mRotationAngle += 0.5f;  // Adjust the speed of rotation as needed

   // Create the model matrix with rotation
   QMatrix4x4 modelMatrix;
   //modelMatrix.rotate(mRotationAngle, QVector3D(1.0f, 0.0f, 0.0f));  // Rotate around the Y-axis
   modelMatrix.rotate(260.0f, QVector3D(1.0f, 0.0f, 0.0f));          // can
   modelMatrix.rotate(mRotationAngle, QVector3D(0.0f, 0.2f, 1.0f));  // can
   //modelMatrix.rotate(175.0f, QVector3D(1.0f, 0.0f, 0.0f));  // for interaction sapce


   float time = mRotationAngle;
   float start = 1.0f;
   float end = 60.0f;
   float duration = 300.0f;  // Duration of one loop cycle

   float t = getLoopingValue(time, duration);
   float interpolatedValue = interpolate(start, end, t);

   //modelMatrix.rotate(interpolatedValue, QVector3D(0.0f, 1.0f, 0.0f));  // for interaction sapce


   // Set the model, view, and projection matrices
   QMatrix4x4 viewMatrix;  // Set this to your view matrix
   viewMatrix.translate(0.0f, 0.5f, -2.5f); // can
  // viewMatrix.translate(0.0f, 0.5f, -0.5f); // for interaction sapce

   mShaderProgram->setUniformValue("model", modelMatrix);
   mShaderProgram->setUniformValue("view", viewMatrix);
   mShaderProgram->setUniformValue("projection", mProjectionMatrix);

   for (size_t i = 0; i < mVAOs.size(); ++i) {
    // Bind the appropriate texture for this part of the mesh
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextureIDs[i]);
    mShaderProgram->setUniformValue("texture_diffuse1", 0);

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


float GltfRenderObject::interpolate(float start, float end, float t) {
  return start + t * (end - start);
}

// Function to get a looping interpolation value
float GltfRenderObject::getLoopingValue(float time, float duration) {
  float phase = fmod(time, duration) / duration;                  // Normalize time to [0, 1]
  return 0.5f * (1.0f + sin(2.0f * M_PI * phase - M_PI / 2.0f));  // Sine wave from 0 to 1
}
}  // namespace nimagna
