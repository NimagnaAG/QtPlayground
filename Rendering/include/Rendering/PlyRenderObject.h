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
class RENDERING_API PlyRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
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
  virtual void draw() override;
  virtual void keyPressEvent(QKeyEvent* event) override {};
  virtual void resizeGL(int w, int h) override {};
  
 protected:
  // the vertex shader code
  static const inline QString mVertexShaderFile = ":/resources/shaders/texture.vert";
  // the fragment shader code
   
  // the shaders
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;
   
  // initialize the shader program
  void setupShaderProgram();
      
};

}  // namespace nimagna
