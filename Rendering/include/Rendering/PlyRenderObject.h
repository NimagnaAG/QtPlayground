#pragma once  
#include <QtCore/QMutex>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFunctions_4_3_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <vector>
#include <string>
#include <memory>
#include "Rendering/pointcloud.h"
#include "gaussiancloud.h"
//#include "pointrenderer.h"
#include "splatrenderer.h"
#include "core/framebuffer.h"
#include "RenderObject.h"



namespace nimagna {

// a render object rendering a textured rectangle, potentially with a separate mask/key/alpha
// texture
class RENDERING_API PlyRenderObject : public RenderObject, protected QOpenGLFunctions_4_3_Core {
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
   
  // draws the render object.
  virtual void draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) override;
  virtual void keyPressEvent(QKeyEvent* event) override {};
  virtual void resizeGL(int w, int h) override {};
  // Static utility functions for PLY file handling
   QString FindConfigFile(const QString& plyFilename, const QString& configFilename);
    QString GetFilenameWithoutExtension(const QString& filepath); 
 
 protected: 
  // the fragment shader code
  std::shared_ptr<PointCloud> pointCloud;
  std::shared_ptr<GaussianCloud> gaussianCloud;
  //std::shared_ptr<PointRenderer> pointRenderer;
  std::shared_ptr<SplatRenderer> splatRenderer;
  std::unique_ptr<QOpenGLShaderProgram> desktopProgram;
  // the shaders
  QString mGsLocation;  // location of the PLY file
  // initialize the shader program
  void setupShaderProgram();
  static void Clear( );
  uint32_t colorTexture;
  std::shared_ptr<FrameBuffer> fbo;
  QMatrix4x4 lastProj, mViewMatrix, gsprojectionMatrix;
  const float znear = 0.2f;
  const float zfar = 1000.0f;
};

}  // namespace nimagna
