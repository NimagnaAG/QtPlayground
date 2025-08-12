#pragma once

#include <Rendering/tiny_gltf.h>

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
class RENDERING_API GltfRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT

 public:
  GltfRenderObject() = delete;
  GltfRenderObject(const QString& texture);
  // not copyable or movable
  GltfRenderObject(const GltfRenderObject& other) = delete;
  GltfRenderObject& operator=(const GltfRenderObject& other) = delete;
  GltfRenderObject(GltfRenderObject&&) = delete;
  GltfRenderObject& operator=(GltfRenderObject&&) = delete;
  virtual ~GltfRenderObject();

  // initializes the render object.
  virtual void initialize() override;

  // draws the render object.
  virtual void draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) override;
  virtual void keyPressEvent(QKeyEvent* event) override {};
  virtual void resizeGL(int w, int h) override {};
 protected:
  // process gltf model
  void processModel(const tinygltf::Model& model);
  void loadTextures(const tinygltf::Model& model);

  // uploads the VertexData to GPU. Must be called after changing mVBD data
  void uploadVertexData();
  // change the mask size
  // update the mask texture data
  // set the position of a particular vertex. does not upload the data to the GPU -> call
  // uploadVertexData after changing the vertex data
  void setVertexPosition(int vertexId, int index, float value);

  // the shaders
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;

 private:
  // the model file location
  const QString mGltfLocation;

  // initialize the shader program
  void setupShaderPrograms();

  // the vertex array objects for the model's meshes
  std::vector<std::unique_ptr<QOpenGLVertexArrayObject>> mVAOs;
  std::vector<int> mIndexCounts;
  std::vector<GLuint> mTextureIDs;
};

}  // namespace nimagna
