#pragma once
#include <QtGui/QKeyEvent>
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include "Rendering/Renderer.h"
#include "Rendering/TextureRenderObject.h"
#include <Rendering/tiny_gltf.h>

namespace nimagna {

class RenderObjectManager;

class OpenGlWidget : public QOpenGLWidget, protected QOpenGLFunctions_4_0_Core {
  Q_OBJECT
 public:
  OpenGlWidget(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
  ~OpenGlWidget();
  // not copyable or movable
  OpenGlWidget(const OpenGlWidget& other) = delete;
  OpenGlWidget& operator=(const OpenGlWidget& other) = delete;
  OpenGlWidget(OpenGlWidget&&) = delete;
  OpenGlWidget& operator=(OpenGlWidget&&) = delete;

  // set the RenderObjectManager
  void setRenderer(std::shared_ptr<Renderer> renderer);
  void enableTrackball(bool enabled);
  void processModel(const tinygltf::Model& model);
  void renderModel();
  //std::vector<QOpenGLVertexArrayObject> mVAOs;
  std::vector<std::unique_ptr<QOpenGLVertexArrayObject>> mVAOs; 
  std::vector<int> mIndexCounts;  // Add this line
  QMatrix4x4 mTransformMatrix;
  void loadTexture(const QString& filePath);

 signals:
  void initialized();

 protected:
  // initialize
  void initializeGL() override;
  // paint
  void paintGL() override;

  // resize
  void resizeGL(int w, int h) override;

  void keyPressEvent(QKeyEvent* event) override;
  void keyReleaseEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private slots:
  void handleResolutionChange(QSize resolution);
  void updateRendering();

 private:
  // the core app and render object manager
  std::shared_ptr<Renderer> mRenderer = nullptr;

  // the texture render object to render the geometry
  std::unique_ptr<TextureRenderObject> mTextureRenderObject;

  bool mTrackballEnabled = true;
  bool mLeftButtonDown = false;
  bool mShiftKeyDown = false;
  bool mFirstDrawOccurred = false;
  QPoint mLastMousePosition = QPoint(0, 0);
  QRect mViewPort;
  RenderData::ShotFraming3D mOriginalTrackballFraming;
};

}  // namespace nimagna
