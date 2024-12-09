#include "Rendering/pch.h"

#include "Rendering/RenderObjectManager.h"

#include <QtCore/QThread>
#include <QtGui/QPainter>
#include <QtOpenGL/QOpenGLPaintDevice>
#include <Rendering/GltfRenderObject.h>

namespace nimagna {

RenderObjectManager::RenderObjectManager() {
  mCurrentRenderData = std::make_shared<RenderData>();
  RenderData::ShotFraming3D framing;
  mCurrentRenderData->setFraming3D(framing);
  mCurrentRenderData->setRenderMode(RenderData::RenderMode::Render3D);
}

RenderObjectManager::~RenderObjectManager() {
  cleanUp();
}

void RenderObjectManager::initialize(std::shared_ptr<QOpenGLContext> context,
                                     std::shared_ptr<QOffscreenSurface> surface) {
  assert(context);
  assert(surface);
  SPDLOG_INFO("Initializing RenderObjectManager");

  mContext = context;
  mOffscreenSurface = surface;
  if (!tryMakeOpenGlContextCurrent(true)) {
    SPDLOG_CRITICAL("Failed to make context current!");
    return;
  }

  SPDLOG_DEBUG("OpenGL extensions:");
  for (const auto& extension : mContext->extensions()) {
    SPDLOG_DEBUG(" - {}", QString(extension));
  }
#if NIMAGNA_WINDOWS
  if (!mContext->hasExtension(QByteArrayLiteral("GL_ARB_multisample"))) {
    SPDLOG_ERROR(
        "Render System: Your GPU does not support multi sampling. This will have an impact "
        "on the quality of the output.");
  }
#endif
#ifdef NIMAGNA_DEBUG
  // turn on debugging if enabled
  changeOpenGlDebugging(true);
#endif
  SPDLOG_INFO("> Create FBO and co.");
  // create FBO
  onOutputSettingsChanged();
  mIsInitialized = true;
  SPDLOG_INFO("> Render once");
  render();
  SPDLOG_INFO("Done initializing RenderObjectManager");
}

void RenderObjectManager::cleanUp() {
  if (!mContext) {
    // already cleaned up
    return;
  }
  SPDLOG_INFO("Clean up RenderObjectManager...");
  if (!tryMakeOpenGlContextCurrent(false)) {
    return;
  }
  // clean up all render objects
  SPDLOG_INFO("> clear objects...");
  clearRenderObjects();
  // release all objects
  if (mRenderFramebuffer) {
    SPDLOG_INFO("> release frame buffer...");
    mRenderFramebuffer->release();
    mRenderFramebuffer.reset(nullptr);
  }
  if (mDebugLogger) {
    SPDLOG_INFO("> stop logging...");
    mDebugLogger->stopLogging();
    mDebugLogger->disconnect();
    mDebugLogger.reset(nullptr);
  }
  if (mContext) {
    SPDLOG_INFO("> release context...");
    mContext.reset();
  }
  SPDLOG_INFO("> DONE");
}

bool RenderObjectManager::tryMakeOpenGlContextCurrent(bool isCritical) {
  if (!mContext) {
    SPDLOG_CRITICAL("No context!");
    return false;
  }
  if (mContext->thread() != QThread::currentThread()) {
    SPDLOG_CRITICAL("Wrong thread!");
    return false;
  }
  if (!mContext->makeCurrent(mOffscreenSurface.get())) {
    if (isCritical) {
      SPDLOG_ERROR(
          "The application failed to create/enable an OpenGL context for rendering the output.");
    }
    SPDLOG_CRITICAL("Context makeCurrent failed!");
    assert(false);
    return false;
  }
  return true;
}

bool RenderObjectManager::render() {
  if (!isInitialized()) return false;

  // activate offscreen context with framebuffer as target
  tryMakeOpenGlContextCurrent(false);
  const bool multisamplingRendering = true;
  if (multisamplingRendering) {
    mMultisampleFramebuffer->bind();
  } else {
    mRenderFramebuffer->bind();
  }

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDepthRange(0.0, 1.0);


  // render objects only if there's render data for the projection and the list has more than one
  // object (i.e. storyboard + more) or the storyboard is the only item and has content
  if (mCurrentRenderData && (mRenderObjectsList.size() > 0)) {
    // get projection from shot
   // const QMatrix4x4 projectionMatrix = mCurrentRenderData->projectionMatrix();
    for (const auto& renderObject : mRenderObjectsList) {
     // renderObject->prepare(projectionMatrix);
      renderObject->draw();
    }
  }

if (multisamplingRendering) {
    // Blit the multisampling framebuffer to the render framebuffer
    QOpenGLFramebufferObject::blitFramebuffer(
        mRenderFramebuffer.get(), mMultisampleFramebuffer.get(), GL_COLOR_BUFFER_BIT,
        GL_LINEAR);  // Blit color buffer with linear filtering

    QOpenGLFramebufferObject::blitFramebuffer(
        mRenderFramebuffer.get(), mMultisampleFramebuffer.get(), GL_DEPTH_BUFFER_BIT,
        GL_NEAREST);  // Blit depth buffer with nearest filtering
  }

  glFlush();
  mRenderFramebuffer->bindDefault();

      GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    SPDLOG_ERROR("OpenGL error: {}", error);
  }

  return true;
}

const RenderObjectManager::RenderObjectList& RenderObjectManager::renderObjects() const {
  return mRenderObjectsList;
}

void RenderObjectManager::clearRenderObjects() {
  mRenderObjectsList.clear();
}

void RenderObjectManager::addTextureObject(const QString& filename) {
  auto qImage = QImage(filename);
  if (qImage.isNull()) {
    SPDLOG_ERROR("No image: {}", filename);
    return;
  }

  std::shared_ptr<TextureRenderObject> renderObject =
      std::make_shared<TextureRenderObject>(TextureRenderObject::kDefaultTextureTarget, qImage);
  renderObject->setDisplayName(filename);

  // add object to data structure
  mRenderObjectsList.emplace_back(renderObject);
}

void RenderObjectManager::addGltfObject(const QString& filename) {
  std::shared_ptr<GltfRenderObject> renderObject =
      std::make_shared<GltfRenderObject>(GltfRenderObject::kDefaultTextureTarget, filename);
  renderObject->setDisplayName(filename);

  // add object to data structure
  mRenderObjectsList.emplace_back(renderObject);
}

void RenderObjectManager::onOutputSettingsChanged() {
  tryMakeOpenGlContextCurrent(false);
  mCurrentOutputResolution = QSize(1080, 720);

  // update render frame buffers (MSAA and texture), local storage and viewport
  QOpenGLFramebufferObjectFormat fboMultisamplingFormat;
  fboMultisamplingFormat.setAttachment(QOpenGLFramebufferObject::Depth);
  fboMultisamplingFormat.setMipmap(true);
  fboMultisamplingFormat.setSamples(8);
  fboMultisamplingFormat.setInternalTextureFormat(GL_RGBA8);
  fboMultisamplingFormat.setTextureTarget(TextureRenderObject::qGlTarget(mRenderFramebufferTarget));
  mMultisampleFramebuffer = std::make_unique<QOpenGLFramebufferObject>(
      mCurrentOutputResolution.width(), mCurrentOutputResolution.height(), fboMultisamplingFormat);

  QOpenGLFramebufferObjectFormat fboDownsampledFormat;
  fboDownsampledFormat.setAttachment(QOpenGLFramebufferObject::Attachment::NoAttachment);
  fboDownsampledFormat.setMipmap(false);
  fboDownsampledFormat.setInternalTextureFormat(GL_RGBA8);
  fboDownsampledFormat.setTextureTarget(TextureRenderObject::qGlTarget(mRenderFramebufferTarget));
  mRenderFramebuffer = std::make_unique<QOpenGLFramebufferObject>(
      mCurrentOutputResolution.width(), mCurrentOutputResolution.height(), fboDownsampledFormat);
  glViewport(0, 0, mCurrentOutputResolution.width(), mCurrentOutputResolution.height());
}

void RenderObjectManager::changeOpenGlDebugging(bool enabled) {
  if (enabled) {
    // setup debug logger
    tryMakeOpenGlContextCurrent(false);
    if (mContext->hasExtension(QByteArrayLiteral("GL_KHR_debug"))) {
      SPDLOG_INFO("Installing OpenGL debug logger...");
      mDebugLogger = std::make_unique<QOpenGLDebugLogger>(this);
      mDebugLogger->initialize();
      // note: the connection is explicitly direct such that the signal handler is executed
      // immediately on the signaling thread. With that, the call stack is visible (when setting a
      // break point) and shows the exact command that caused the OpenGL message.
      connect(mDebugLogger.get(), &QOpenGLDebugLogger::messageLogged, this,
              &RenderObjectManager::onOpenGlDebugMessage,
              static_cast<Qt::ConnectionType>(Qt::DirectConnection | Qt::UniqueConnection));
#ifdef NIMAGNA_RELEASE
      // this will just print messages to the log but not allow to trace them back
      mDebugLogger->startLogging(QOpenGLDebugLogger::LoggingMode::AsynchronousLogging);
#else
      // this allows to see the stack when a log message is emitted but lowers the performance
      mDebugLogger->startLogging(QOpenGLDebugLogger::LoggingMode::SynchronousLogging);
#endif
    }
  } else {
    if (mDebugLogger) {
      mDebugLogger->stopLogging();
      mDebugLogger->disconnect();
      mDebugLogger.reset(nullptr);
    }
  }
}

void RenderObjectManager::onOpenGlDebugMessage(const QOpenGLDebugMessage& debugMessage) {
  // count messages and show messages less frequent if they appear regularly
  auto count = mDebugMessageIdCounter[debugMessage.id()];
  mDebugMessageIdCounter[debugMessage.id()] = ++count;
  if ((count > 10 && count <= 100) && (count % 10 != 0)) {
    return;
  }
  if ((count > 100) && (count % 100 != 0)) {
    return;
  }
  switch (debugMessage.severity()) {
    case QOpenGLDebugMessage::HighSeverity:
      SPDLOG_ERROR("OpenGl error: {}, source={}, id={}, type={}, count={}", debugMessage.message(),
                   debugMessage.source(), debugMessage.id(), debugMessage.type(), count);
      break;
    case QOpenGLDebugMessage::MediumSeverity:
      SPDLOG_WARN("OpenGl warning: {}, source={}, id={}, type={}, count={}", debugMessage.message(),
                  debugMessage.source(), debugMessage.id(), debugMessage.type(), count);
      break;
    case QOpenGLDebugMessage::LowSeverity:
      SPDLOG_INFO("OpenGl info: {}, source={}, id={}, type={}, count={}", debugMessage.message(),
                  debugMessage.source(), debugMessage.id(), debugMessage.type(), count);

      break;
    case QOpenGLDebugMessage::NotificationSeverity:
      SPDLOG_DEBUG("OpenGl notification: {}, id={}, type={}, count={}", debugMessage.message(),
                   debugMessage.source(), debugMessage.id(), debugMessage.type(), count);
      break;
    default:
      SPDLOG_INFO("OpenGl other: {}, source={}, id={}, type={}, count={}", debugMessage.message(),
                  debugMessage.source(), debugMessage.id(), debugMessage.type(), count);
      break;
  }
}

int RenderObjectManager::getRenderObjectRowIndex(
    const std::shared_ptr<RenderObject>& object) const {
  if (object) {
    if (const auto iter = std::find(mRenderObjectsList.begin(), mRenderObjectsList.end(), object);
        iter != mRenderObjectsList.end()) {
      auto index = static_cast<int>(std::distance(mRenderObjectsList.begin(), iter));
      return index;
    }
  }
  return -1;
}

int RenderObjectManager::renderObjectListCount() const {
  return static_cast<int>(mRenderObjectsList.size());
}

}  // namespace nimagna
