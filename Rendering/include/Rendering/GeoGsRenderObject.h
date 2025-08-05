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
  GeoGsRenderObject() = delete;
  GeoGsRenderObject(const QString& location);
  // not copyable or movable
  GeoGsRenderObject(const GeoGsRenderObject& other) = delete;
  GeoGsRenderObject& operator=(const GeoGsRenderObject& other) = delete;
  GeoGsRenderObject(GeoGsRenderObject&&) = delete;
  GeoGsRenderObject& operator=(GeoGsRenderObject&&) = delete;
  virtual ~GeoGsRenderObject();
  // initializes the render object.
  virtual void initialize() override;

  // draws the render object.
  virtual void draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) override;

 private:
  void initializeGL();
  std::vector<unsigned int> generateTexture();

  // the file to be loaded
  const QString mGsLocation = "";

  // the data structure to hold the splat data from loading
  struct SplatData {
    std::vector<QVector3D> positions;
    std::vector<QVector3D> scales;
    std::vector<QVector4D> rotations;
    std::vector<QVector4D> colors;
  };
  SplatData mSplatData;
  // loading functions
  SplatData loadSplatFile(const QString& filePath);
  void LoadSplatGs(const QString& location);
  void LoadAnimateGs(const QString& location) {};

  // sort splats and update index buffer
  void sortSplatsAndUpdateIndexBufferObject(const QMatrix4x4& viewProj);
  void RunSort(const QMatrix4x4& viewProj);
  // the shader program
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;
  // the locations of the view and projection matrices in the shader
  int m_uViewLoc, m_uProjLoc;
  int viewportLocation, focalPosition;
  QMatrix4x4 lastProj, mViewMatrix;
  // initialize the shader program
  void setupShaderProgram();
  // the Vertex Array Object holds all vertex relevant data
  QOpenGLVertexArrayObject mVAO;
  // the vertex buffer object
  QOpenGLBuffer mVBO;
  // the index buffer with the vertex indices for each triangle
  QOpenGLBuffer mIBO;
  int vertexCount = 0; 
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
  QPair<float, float> calculateFocalLengths(float verticalFovDegrees, float width, float height) {
    float fovYRad = qDegreesToRadians(verticalFovDegrees); 
    // Compute fy based on vertical FOV
    float fy = height / (2.0f * qTan(fovYRad / 2.0f));

    // Derive fx from fy and aspect ratio
    float aspect = width / height;
    float fx = fy * aspect;

    return qMakePair(fx, fy);
  }
  void resizeGL(int w, int h ) {  
     mShaderProgram->bind();
     auto [fx, fy] = calculateFocalLengths(fovY(), static_cast<float>(w), static_cast<float>(h)); 
     QVector2D focalValue(fx, fy); 
     mShaderProgram->setUniformValue(focalPosition, focalValue);  
     QMatrix4x4 gsprojectionMatrix = getProjectionMatrix(fx, fy, w, h);

     mShaderProgram->setUniformValue(m_uProjLoc, gsprojectionMatrix);
     QSize viewportSize = QOpenGLContext::currentContext()->surface()->size();
     mShaderProgram->setUniformValue(viewportLocation,  QVector2D(viewportSize.width(), viewportSize.height()));

     mShaderProgram->setUniformValue(m_uViewLoc, mViewMatrix);
     sortSplatsAndUpdateIndexBufferObject(mViewMatrix * gsprojectionMatrix);
     SPDLOG_INFO("OpenGlWidget::resizeGL {}, {}, viewportSize {}, {}, focal {}, {}", w, h, viewportSize.width(), viewportSize.height(), fx, fy);
   }
};

}  // namespace nimagna
