#pragma once
#include <QByteArray>
#include <QDebug>
#include <QFile>
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
#include <cmath>
#include <fstream>
#include <limits>
#include <vector> 
#include "RenderObject.h"
#include "Rendering/Rendering.h"
namespace nimagna { 
class RENDERING_API GsRenderObject : public RenderObject, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT

  friend class OpenGlWidget;

 public:
  struct Camera {
    int id;
    QString img_name;
    int width;
    int height;
    QVector3D position;
    QMatrix3x3 rotation;  // Store as 3x3 for convenience
    float fy;
    float fx;
  };

  GsRenderObject() = delete;
  GsRenderObject(const QString& location);
  // not copyable or movable
  GsRenderObject(const GsRenderObject& other) = delete;
  GsRenderObject& operator=(const GsRenderObject& other) = delete;
  GsRenderObject(GsRenderObject&&) = delete;
  GsRenderObject& operator=(GsRenderObject&&) = delete;
  virtual ~GsRenderObject() {
    mShaderProgram.reset(); 
    mVBO.destroy();
  }
  void LoadSplatGs(const QString& location);
  void LoadAnimateGs(const QString& location);
  // initializes the render object.
  virtual void initialize() override;
  // draws the render object.
  virtual void draw() override;
  // initialize the shader program
  void setupShaderProgram();
  void resizeGL(int w, int h);
  QVector<uint32_t> RunSort(QByteArray buffer);
  QVector<quint32> generateTexture(QByteArray buffer);
 protected:
  static const inline QString mVertexShaderFile = ":/resources/shaders/AnimateGS.vert";
  static const inline QString mFragmentShaderFile = ":/resources/shaders/AnimateGS.frag";
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;
  QString mGsLocation = "";

  Camera m_camera;
  int vertexCount = 0;
  int LastVertexCount = -1;
  QMatrix4x4 viewProj;
  QMatrix4x4 lastProj;

  GLint m_aPositionLoc, m_aIndexLoc;
 
  QVector<quint32> mTextureData;
  float mDownsample; 
  std::unique_ptr<QOpenGLTexture> mTexture;

  QOpenGLVertexArrayObject mVAO;
  // the vertex buffer object
  QOpenGLBuffer mVBO;
  // the index buffer with the vertex indices for each triangle
  QOpenGLBuffer mIBO;
 

    quint16 floatToHalf(float value) {
    // Use built-in half if available (Qt 6.6+) or implement manually.
    union {
      float f;
      uint32_t u;
    } v = {value};
    uint32_t f = v.u;
    int sign = (f >> 16) & 0x8000;
    int exponent = ((f >> 23) & 0xFF) - 127 + 15;
    int mantissa = (f >> 13) & 0x3FF;

    if (exponent <= 0) return sign;
    if (exponent >= 31) return sign | 0x7C00;
    return sign | (exponent << 10) | mantissa;
  }

  quint32 packHalf2x16(float a, float b) { return (floatToHalf(b) << 16) | floatToHalf(a); }
  QMatrix4x4 getProjectionMatrix(float fx, float fy, int width, int height) {
    const float znear = 0.2f;
    const float zfar = 200.0f;
    QMatrix4x4 projection;
    projection.setColumn(0, {2 * fx / width, 0, 0, 0});
    projection.setColumn(1, {0, -2 * fy / height, 0, 0});
    projection.setColumn(2, {0, 0, zfar / (zfar - znear), 1});
    projection.setColumn(3, {0, 0, -(zfar * znear) / (zfar - znear), 0});
    return projection;
  }
};

}  // namespace nimagna
