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
  mTextureRenderObject->setEnableDepthTest(false);
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setVersion(4, 0);
  format.setSamples(8);
  mOrthographic2DFraming = std::make_shared<RenderData>();
  mOrthographic2DFraming->setRenderMode(RenderData::RenderMode::Render2D);
  setFormat(format);
  setFocusPolicy(Qt::StrongFocus);
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
  mTextureRenderObject->setFlipVertically(false);
  mTextureRenderObject->useExternalTexture(true);
  mTextureRenderObject->setFlipHorizontally(false);

  // update resolution and connect to settings change
  // the resolution of the texture of the offscreen rendered image is fixed to 1080p for now
  handleResolutionChange(mRenderer->renderObjectManager()->currentOutputResolution());
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
  glBlendFunc(GL_ONE, GL_ZERO);

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
  mTextureRenderObject->draw(mOrthographic2DFraming);
  glActiveTexture(GL_TEXTURE0);

  if (!mFirstDrawOccurred) {
    // once the first time the buffer is drawn, emit the initialized signal
    emit initialized();
  }
  mFirstDrawOccurred = true;
}

void OpenGlWidget::paintEvent(QPaintEvent* event) {
  QOpenGLWidget::paintEvent(event);

  if (mDrawDebugInfo) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto rd = mRenderer->renderObjectManager()->currentRenderData();
    const auto viewPos = 20;
    painter.drawText(10, viewPos, "View");
    auto v = rd->viewMatrix().constData();
    for (auto r = 0; r < 4; ++r) {
      painter.drawText(10, viewPos + 20 + r * 20,
                       QString("%1 %2 %3 %4")
                           .arg(v[r * 4 + 0], 0, 'f', 3)
                           .arg(v[r * 4 + 1], 0, 'f', 3)
                           .arg(v[r * 4 + 2], 0, 'f', 3)
                           .arg(v[r * 4 + 3], 0, 'f', 3));
    }
    painter.drawText(10, viewPos + 100, QString("FOV: %1").arg(rd->fieldOfViewAngle()));
    const auto projPos = viewPos + 120;
    painter.drawText(10, projPos, "Projection");
    v = rd->projectionMatrix().constData();
    for (auto r = 0; r < 4; ++r) {
      painter.drawText(10, projPos + 20 + r * 20,
                       QString("%1 %2 %3 %4")
                           .arg(v[r * 4 + 0], 0, 'f', 3)
                           .arg(v[r * 4 + 1], 0, 'f', 3)
                           .arg(v[r * 4 + 2], 0, 'f', 3)
                           .arg(v[r * 4 + 3], 0, 'f', 3));
    }
    painter.drawText(
        10, projPos + 100,
        QString("Viewport: %1x%2").arg(rd->viewport().width()).arg(rd->viewport().height()));
  }
}

void OpenGlWidget::resizeGL(int w, int h) {
  // needs pixel ratio for high dpi displays
  double pixelRatio = devicePixelRatioF();
  //  Viewport: present ROM framebuffer in a centered 16/9 format
  const float widgetRatio = static_cast<float>(width()) / height();
  const auto outputResolution = mRenderer->renderObjectManager()->currentOutputResolution();
  const float outputRatio =
      outputResolution.width() / static_cast<float>(outputResolution.height());
  if (widgetRatio > outputRatio) {
    // width bigger
    int newWidth = outputRatio * height();
    mViewPort.setX(pixelRatio * (width() - newWidth) / 2);
    mViewPort.setY(0);
    mViewPort.setHeight(pixelRatio * height());
    mViewPort.setWidth(pixelRatio * newWidth);
  } else {
    // height bigger
    int newHeight = width() / outputRatio;
    mViewPort.setX(0);
    mViewPort.setY(pixelRatio * (height() - newHeight) / 2);
    mViewPort.setWidth(pixelRatio * width());
    mViewPort.setHeight(pixelRatio * newHeight);
  }
  QOpenGLWidget::resizeGL(w, h);
  mOrthographic2DFraming->setViewport(mViewPort.size());
  SPDLOG_INFO("ResizeGL: {}x{} (viewport: {}x{}/{}x{})", w, h, mViewPort.x(), mViewPort.y(),
              mViewPort.width(), mViewPort.height());
}

void OpenGlWidget::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Shift) {
    mShiftKeyDown = true;
  }
  const float stepLength = 0.1f;
  const float rotLength = 1.0f;
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;
  const auto& renderData = rom->currentRenderData();
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    auto viewMatrix = rom->currentRenderData()->viewMatrix();
    auto inverseViewMatrix = viewMatrix.inverted();
    switch (event->key()) {
      case Qt::Key_W:
        inverseViewMatrix.translate(0, 0, -stepLength);
        break;
      case Qt::Key_S:
        inverseViewMatrix.translate(0, 0, stepLength);
        break;
      case Qt::Key_A:
        inverseViewMatrix.translate(-stepLength, 0, 0);
        break;
      case Qt::Key_D:
        inverseViewMatrix.translate(stepLength, 0, 0);
        break;
      case Qt::Key_Q:
        inverseViewMatrix.translate(0, -stepLength, 0);
        break;
      case Qt::Key_E:
        inverseViewMatrix.translate(0, stepLength, 0);
        break;
      case Qt::Key_Up:
        inverseViewMatrix.rotate(rotLength, {1, 0, 0});
        break;
      case Qt::Key_Down:
        inverseViewMatrix.rotate(-rotLength, {1, 0, 0});
        break;
      case Qt::Key_Left:
        inverseViewMatrix.rotate(rotLength, {0, 1, 0});
        break;
      case Qt::Key_Right:
        inverseViewMatrix.rotate(-rotLength, {0, 1, 0});
        break;
      default:
        break;
    }
    rom->currentRenderData()->setViewMatrix(inverseViewMatrix.inverted());

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
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    if (event->button() == Qt::LeftButton) {
      mLeftButtonDown = true;
    } else if (event->button() == Qt::RightButton) {
      mRightButtonDown = true;
    }
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
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    if (event->button() == Qt::LeftButton) {
      mLeftButtonDown = false;
    } else if (event->button() == Qt::RightButton) {
      mRightButtonDown = false;
    }
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
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    const auto globalPosition = event->globalPosition();
    float differenceX = (globalPosition.x() - mLastMousePosition.x());
    float differenceY = (globalPosition.y() - mLastMousePosition.y());

    auto viewMatrix = rom->currentRenderData()->viewMatrix();
    auto inverseViewMatrix = viewMatrix.inverted();
    if (mRightButtonDown) {
      float factor = 0.025f;
      inverseViewMatrix.translate(-factor * differenceX, -factor * differenceY, 0);
    } else if (mLeftButtonDown) {

      float factor = 0.1f;
      QQuaternion quat =
          QQuaternion::fromEulerAngles({factor * differenceY, factor * differenceX, 0});
      inverseViewMatrix.rotate(quat);
    }
    rom->currentRenderData()->setViewMatrix(inverseViewMatrix.inverted());

    mLastMousePosition = globalPosition.toPoint();
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
    auto framing3D = renderData->framing3D();
    float angle = framing3D.fieldOfViewAngle();
    angle += delta.y() * changeFactor;
    framing3D.setFieldOfViewAngle(std::clamp(angle, 2.f, 180.f));
    SPDLOG_INFO("Change view angle to {}", angle);
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