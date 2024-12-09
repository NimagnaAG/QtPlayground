#include "pch.h"

#include "OpenGlWidget.h"

#include <QtCore/QRandomGenerator>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>

namespace nimagna {

OpenGlWidget::OpenGlWidget(QWidget* parent /*= nullptr*/, Qt::WindowFlags f /*= Qt::WindowFlags()*/)
    : QOpenGLWidget(parent, f) {
  mTextureRenderObject =
      std::make_unique<TextureRenderObject>(TextureRenderObject::kDefaultTextureTarget);
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setVersion(4, 0);
  format.setSamples(8);
  setFormat(format);
  setMouseTracking(true);
}

OpenGlWidget::~OpenGlWidget() {
  // the texture render object needs a current context for destruction
  context()->makeCurrent(context()->surface());
  mTextureRenderObject.reset();
}

void OpenGlWidget::setRenderer(std::shared_ptr<Renderer> renderer) {
  if (mRenderer) {
    disconnect(mRenderer.get(), nullptr, this, nullptr);
  }
  mRenderer = std::move(renderer);
  if (mRenderer) {
    connect(mRenderer.get(), &Renderer::renderFrameUpdated, this, [this]() { update(); });
  }
}

void OpenGlWidget::enableTrackball(bool enabled) {
  if (enabled) {
    // trackball turned on: remember current 3D framing
    mOriginalTrackballFraming = mRenderer->renderObjectManager()->currentRenderData()->framing3D();
  } else if (mTrackballEnabled && !enabled) {
    // trackball gets switched off: check to reset framing
    if (QMessageBox::question(nullptr, "3D Framing", "Do you want to keep this 3D framing?") ==
        QMessageBox::StandardButton::No) {
      // reset framing
      mRenderer->renderObjectManager()->currentRenderData()->setFraming3D(
          mOriginalTrackballFraming);
    }
  }
  mTrackballEnabled = enabled;
}

void OpenGlWidget::initializeGL() {
  SPDLOG_INFO("Initializing OpenGlWidget");
  initializeOpenGLFunctions();
  // initialize the texture render object with all the OpenGL stuff
  mTextureRenderObject->initialize();
  auto outputResolution = QSize(1080, 720);
  mTextureRenderObject->changeTextureSizeAndFormat(outputResolution,
                                                   mTextureRenderObject->sourcePixelFormat());
  mTextureRenderObject->setFlipVertically(true);
  mTextureRenderObject->useExternalTexture(true);
  mTextureRenderObject->setFlipHorizontally(false);

  // update resolution and connect to settings change
  handleResolutionChange(outputResolution);
  winId();  // required to get correct pixel ratio for high dpi setups
  glEnable(GL_MULTISAMPLE);

  SPDLOG_WARN("Active viewport renderer: {}", this->glGetString(GL_RENDERER));
}

void OpenGlWidget::paintGL() {
  // bind default framebuffer
  QOpenGLFramebufferObject::bindDefault();
  glEnable(GL_MULTISAMPLE);

  // setup output and background
  glViewport(mViewPort.x(), mViewPort.y(), mViewPort.width(), mViewPort.height());
  glClearColor(0.f, 0.f, 0.f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // render RenderObjectManager's framebuffer as texture to screen
  // only paint if there's a render object manager and it is initialized
  if (mRenderer && mRenderer->renderObjectManager() &&
      mRenderer->renderObjectManager()->isInitialized()) {
    glActiveTexture(GL_TEXTURE0 + mTextureRenderObject->colorTextureUnit());
    glBindTexture(
        TextureRenderObject::glTarget(mRenderer->renderObjectManager()->renderFrameBufferType()),
        mRenderer->renderObjectManager()->renderFrameBuffer()->texture());
  }
  // render texture object without using its texture
  mTextureRenderObject->draw();
  glActiveTexture(GL_TEXTURE0);

  if (!mFirstDrawOccurred) {
    // once the first time the buffer is drawn, emit the initialized signal
    emit initialized();
  }
  mFirstDrawOccurred = true;
}

void OpenGlWidget::resizeGL(int w, int h) {
  // needs pixel ratio for high dpi displays
  double pixelRatio = devicePixelRatioF();
  // Viewport: present framebuffer in a centered 16/9 format
  float ratio = static_cast<float>(width()) / height();
  if (ratio > (16.f / 9.f)) {
    // width bigger
    int newWidth = 16.f * height() / 9.f;
    mViewPort.setX(pixelRatio * (width() - newWidth) / 2);
    mViewPort.setY(0);
    mViewPort.setHeight(pixelRatio * height());
    mViewPort.setWidth(pixelRatio * newWidth);
  } else {
    // height bigger
    int newHeight = 9.f * width() / 16.f;
    mViewPort.setX(0);
    mViewPort.setY(pixelRatio * (height() - newHeight) / 2);
    mViewPort.setWidth(pixelRatio * width());
    mViewPort.setHeight(pixelRatio * newHeight);
  }
  QOpenGLWidget::resizeGL(w, h);
}

void OpenGlWidget::keyPressEvent(QKeyEvent* event) { 
    if (event->key() == Qt::Key_Shift) {
    mShiftKeyDown = true;
  }
  const float stepLength = 0.1f;
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;
  const auto& renderData = rom->currentRenderData();
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    RenderData::ShotFraming3D framing3D = rom->currentRenderData()->framing3D();
    QVector3D vector = framing3D.position();
    if (mShiftKeyDown) {
      vector = framing3D.lookAtPoint();
    }
    if (event->key() == Qt::Key_W) vector += QVector3D(0, 0, -stepLength);
    if (event->key() == Qt::Key_S) vector += QVector3D(0, 0, stepLength);
    if (event->key() == Qt::Key_A) vector += QVector3D(-stepLength, 0, 0);
    if (event->key() == Qt::Key_D) vector += QVector3D(stepLength, 0, 0);
    if (event->key() == Qt::Key_Q) vector += QVector3D(0, stepLength, 0);
    if (event->key() == Qt::Key_E) vector += QVector3D(0, -stepLength, 0);
    event->accept();
    if (mShiftKeyDown) {
      framing3D.setLookAtPoint(vector);
    } else {
      framing3D.setPosition(vector);
    }
    rom->currentRenderData()->setFraming3D(framing3D);
    updateRendering();
  }
}

void OpenGlWidget::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Shift) {
    mShiftKeyDown = false;
  }
}

void OpenGlWidget::mousePressEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && renderData && renderData->is3D() && event->button() == Qt::LeftButton) {
    mLeftButtonDown = true;
    mLastMousePosition = event->globalPosition().toPoint();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mousePressEvent(event);
  }
}

void OpenGlWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && renderData && renderData->is3D() && event->button() == Qt::LeftButton) {
    mLeftButtonDown = false;
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mouseReleaseEvent(event);
  }
}

void OpenGlWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && mLeftButtonDown && renderData && renderData->is3D()) {
    // left button pressed (only in 3D)
    float factor = 0.035f;
    float differenceX = factor * (event->globalPosition().x() - mLastMousePosition.x());
    float differenceY = factor * (event->globalPosition().y() - mLastMousePosition.y());
    RenderData::ShotFraming3D framing3D = renderData->framing3D();
    QVector3D position = framing3D.position();
    position += QVector3D(differenceX, differenceY, 0);
    mLastMousePosition = event->globalPosition().toPoint();
    framing3D.setPosition(position);
    renderData->setFraming3D(framing3D);
    updateRendering();
    event->accept();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mouseMoveEvent(event);
  }
}

void OpenGlWidget::wheelEvent(QWheelEvent* event) {
  event->ignore();
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    QPointF delta = event->angleDelta();
    const float changeFactor = 1 / 20.f;
    RenderData::ShotFraming3D framing3D = renderData->framing3D();
    float angle = framing3D.fieldOfViewAngle();
    angle += delta.y() * changeFactor;
    const float minViewAngle = 5.f;
    const float maxViewAngle = 80.f;
    if (angle < minViewAngle) angle = minViewAngle;
    if (angle > maxViewAngle) angle = maxViewAngle;
    framing3D.setFieldOfViewAngle(angle);
    renderData->setFraming3D(framing3D);
    updateRendering();
    event->accept();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::wheelEvent(event);
  }
}

void OpenGlWidget::handleResolutionChange(QSize resolution) {
  makeCurrent();
  mTextureRenderObject->changeTextureSizeAndFormat(resolution,
                                                   mTextureRenderObject->sourcePixelFormat());
}

void OpenGlWidget::updateRendering() {
  update();
}

}  // namespace nimagna