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

#include <cmath>

namespace nimagna {

GltfRenderObject::GltfRenderObject(const QString& location) : mGltfLocation(location) {
  initialize();
}

GltfRenderObject::~GltfRenderObject() {
  // clean up VAOs
  for (const auto& vao : mVAOs) {
    vao->destroy();
  }
  // clean up textures
  for (const auto textureId : mTextureIDs) {
    glDeleteTextures(1, &textureId);
  }
  // clean up shader
  mShaderProgram.reset();
}

void GltfRenderObject::initialize() {
  initializeOpenGLFunctions();

  // load model first
  SPDLOG_INFO("Initializing GltfRenderObject with location: {}", mGltfLocation);

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

  // initialize model matrix
  auto modelMatrix = getModelMatrix();
  modelMatrix.setToIdentity();
  modelMatrix.scale(0.5f);
  // modelMatrix.rotate(270, {1.f, 0.f, 0.f});
  setModelMatrix(modelMatrix);

  // load and compile shader
  setupShaderPrograms();

  // Process the model (e.g., create OpenGL buffers)
  processModel(model);

  // load texture if available
  if (model.materials.empty() || model.textures.empty() || model.images.empty()) {
    SPDLOG_WARN("No textures found in the model.");
  } else {
    loadTextures(model);
  }

  // Done
  RenderObject::initialize();
}

void GltfRenderObject::processModel(const tinygltf::Model& model) {
  struct Vertex {
    QVector3D position;
    QVector3D normal;
    QVector2D texCoords;
  };

  std::vector<Vertex> vertices;
  std::vector<GLuint> indices;

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      auto vao = std::make_unique<QOpenGLVertexArrayObject>();
      if (!vao->create()) {
        SPDLOG_ERROR("Failed to create VertexArrayObject");
        continue;
      }
      vao->bind();

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
        vertex.position = QVector3D(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);

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

void GltfRenderObject::setupShaderPrograms() {
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
}

void GltfRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  if (!mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }

  // no clearing of framebuffer!

  // set up rendering for the GLTF model
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);

  // bind and update shader program
  mShaderProgram->bind();
  mShaderProgram->setUniformValue("model", getModelMatrix());
  mShaderProgram->setUniformValue("view", viewMatrix);
  mShaderProgram->setUniformValue("projection", projectionMatrix);

  // render each mesh
  for (size_t i = 0; i < mVAOs.size(); ++i) {
    if (mTextureIDs.size() > i) {
      // Bind the appropriate texture for this part of the mesh
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, mTextureIDs[i]);
      mShaderProgram->setUniformValue("texture_diffuse1", 0);
    }

    mVAOs[i]->bind();
    glDrawElements(GL_TRIANGLES, mIndexCounts[i], GL_UNSIGNED_SHORT, 0);
    mVAOs[i]->release();
  }

  // clean up
  glBindTexture(GL_TEXTURE_2D, 0);
  mShaderProgram->release();
}

}  // namespace nimagna
