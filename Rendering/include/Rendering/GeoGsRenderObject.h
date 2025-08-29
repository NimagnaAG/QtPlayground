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
  virtual void draw(const std::shared_ptr<RenderData> renderData) override;

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
  void loadSplatGs(const QString& location);
  void loadAnimateGs(const QString& location) {};

  // the shader program
  std::unique_ptr<QOpenGLShaderProgram> mShaderProgram;
  // the locations of the view and projection matrices in the shader
  int mShaderViewMatrixLocation, mShaderProjectionMatrixLocation;
  int mShaderViewportLocation, mShaderFocalPosition;

  // initialize the shader program
  void setupShaderProgram();
  // the Vertex Array Object holds all vertex relevant data
  QOpenGLVertexArrayObject mVAO;
  // the vertex buffer object
  QOpenGLBuffer mVBO;
  // the index buffer with the vertex indices for each triangle
  QOpenGLBuffer mIBO;

  QMatrix4x4 mLastViewProjectionMatrix;
  void updateIfViewProjectionChanged(const std::shared_ptr<RenderData> renderData);
  QPair<float, float> calculateFocalLengths(float verticalFovDegrees, float width, float height);
  // sort splats and update index buffer
  void sortSplatsAndUpdateIndexBufferObject(const QMatrix4x4& viewProj);
};

}  // namespace nimagna
