#pragma once
#include <QtCore/QMutex>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFunctions_4_5_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <memory>
#include <string>
#include <vector>

#include "Rendering/pointcloud.h"
#include "gaussiancloud.h"
// #include "splatrenderer.h"
#include "RenderObject.h"
#include "Renderer.h"
#include "Rendering/Rendering.h"
#include "core/framebuffer.h"

#define GL_GLEXT_PROTOTYPES
#include <qvectornd.h>
#include <stdint.h>

#include <QOpenGLExtraFunctions>
#include <QVector>
#include <glm/glm.hpp>

#include "Rendering/radix_sort.hpp"
#include "core/program.h"
#include "core/util.h"
#include "core/vertexbuffer.h"
namespace nimagna {

// a render object rendering a textured rectangle, potentially with a separate mask/key/alpha
// texture
class RENDERING_API PlyRenderObject : public RenderObject, protected QOpenGLFunctions_4_5_Core {
  Q_OBJECT

  friend class OpenGlWidget;

 public:
  PlyRenderObject() = delete;
  PlyRenderObject(const QString& location);
  // not copyable or movable
  PlyRenderObject(const PlyRenderObject& other) = delete;
  PlyRenderObject& operator=(const PlyRenderObject& other) = delete;
  PlyRenderObject(PlyRenderObject&&) = delete;
  PlyRenderObject& operator=(PlyRenderObject&&) = delete;
  virtual ~PlyRenderObject();

  // initializes the render object.
  virtual void initialize() override;
  void BuildVertexArrayObject();
  // draws the render object.
  virtual void draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) override;

  QString FindConfigFile(const QString& plyFilename, const QString& configFilename);
  QString GetFilenameWithoutExtension(const QString& filepath);
  uint32_t numBlocksPerWorkgroup = 1024;
  void SetupAttrib(int loc, const BinaryAttribute& attrib, int32_t count, size_t stride) {
    assert(attrib.type == BinaryAttribute::Type::Float);
    glVertexAttribPointer(loc, count, GL_FLOAT, GL_FALSE, (uint32_t)stride, (void*)attrib.offset);
    glEnableVertexAttribArray(loc);
  }

  void Sort(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat, const QVector2D& nearFar);

 protected:
  // the fragment shader code
  std::shared_ptr<GaussianCloud> gaussianCloud;
  // the shaders
  QString mGsLocation;  // location of the PLY file
  // initialize the shader program
  void setupShaderProgram() {};
  void Clear();
  uint32_t colorTexture;
  QMatrix4x4 lastProj, mViewMatrix;
  const float znear = 0.2f;
  const float zfar = 1000.0f;
  std::shared_ptr<rgc::radix_sort::sorter> sorter;
  std::unique_ptr<QOpenGLShaderProgram> splatProg;
  std::unique_ptr<QOpenGLShaderProgram> preSortProg;
  std::unique_ptr<QOpenGLShaderProgram> histogramProg;
  std::shared_ptr<QOpenGLShaderProgram> sortProg;
  QOpenGLVertexArrayObject splatVao;

  std::vector<uint32_t> indexVec;
  std::vector<uint32_t> depthVec;
  std::vector<glm::vec4> posVec;
  std::vector<uint32_t> atomicCounterVec;
  std::shared_ptr<BufferObject> gaussianDataBuffer;
  std::shared_ptr<BufferObject> keyBuffer;
  std::shared_ptr<BufferObject> keyBuffer2;
  std::shared_ptr<BufferObject> histogramBuffer;
  std::shared_ptr<BufferObject> valBuffer;
  std::shared_ptr<BufferObject> valBuffer2;
  std::shared_ptr<BufferObject> posBuffer;
  std::shared_ptr<BufferObject> atomicCounterBuffer;
  uint32_t sortCount;
};

}  // namespace nimagna
