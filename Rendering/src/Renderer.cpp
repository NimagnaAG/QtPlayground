#include "Rendering/pch.h"

#include "Rendering/Renderer.h"

namespace nimagna {

/* ******************************************************************
  RenderWorker
 ****************************************************************** */

RenderWorker::RenderWorker() {}

std::shared_ptr<nimagna::RenderObjectManager> RenderWorker::renderObjectManager() const {
  return mRenderObjectManager;
}

void RenderWorker::initializeRendering() {
  SPDLOG_INFO("Create ROM");
  // create the render object manager in the rendering thread...
  mRenderObjectManager = std::make_shared<RenderObjectManager>();
}

void RenderWorker::startRendering(std::shared_ptr<QOpenGLContext> context,
                                  std::shared_ptr<QOffscreenSurface> surface) {
  assert(context);
  assert(surface);
  SPDLOG_INFO("Start rendering");
  // initialize the ROM with the given context and surface
  mRenderObjectManager->initialize(context, surface);
  // and start the timer to trigger renderings
  createAndStartTimerIfNeeded();
  // set active flag
  mIsActive = true;
}

void RenderWorker::stopRendering() {
  SPDLOG_INFO("Stop rendering");
  // unset active flag
  mIsActive = false;
  if (mTimer) {
    // stop and reset the timer
    mTimer->stop();
    mTimer.reset(nullptr);
  }
  // clear and destroy ROM
  mRenderObjectManager->cleanUp();
  mRenderObjectManager.reset();
}

void RenderWorker::loadImage(QString filename) {
  if (!mRenderObjectManager) return;
  mRenderObjectManager->addTextureObject(filename);
}

void RenderWorker::loadGLTF(QString filename) {
  SPDLOG_INFO("Load GLTF called");
  if (!mRenderObjectManager) return;

  SPDLOG_INFO("A GLTF Should be displayed now..: " + filename);
  // add gltf viewer / loader here
}

void RenderWorker::render() {
  // slot called by the timer to trigger a render iteration
  if (!mRenderObjectManager || !mRenderObjectManager->isInitialized()) return;
  // render
  if (mRenderObjectManager->render()) {
    emit renderFrameReady();
  }
}

void RenderWorker::createAndStartTimerIfNeeded() {
  if (mTimer) return;
  mTimer = std::make_unique<QTimer>();
  udpateOutputFps();
  connect(mTimer.get(), &QTimer::timeout, this, &RenderWorker::render);
  mTimer->setTimerType(Qt::PreciseTimer);
  mTimer->start();
}

void RenderWorker::udpateOutputFps() {
  auto outputFps = 30;
  SPDLOG_INFO("Set renderer FPS to {}", outputFps);
  mTimer->setInterval(1000 / outputFps);
}

/* ******************************************************************
  Renderer
 ****************************************************************** */

Renderer::Renderer() {
  // create the render worker and connect the signals
  SPDLOG_INFO("Create RenderWorker..");
  mRenderWorker = std::make_unique<RenderWorker>();
  connect(this, &Renderer::initializeRenderer, mRenderWorker.get(),
          &RenderWorker::initializeRendering);
  connect(this, &Renderer::startRenderer, mRenderWorker.get(), &RenderWorker::startRendering);
  connect(this, &Renderer::stopRenderer, mRenderWorker.get(), &RenderWorker::stopRendering);
  connect(this, &Renderer::loadImage, mRenderWorker.get(), &RenderWorker::loadImage);
  connect(this, &Renderer::loadGLTF, mRenderWorker.get(), &RenderWorker::loadGLTF);
  connect(mRenderWorker.get(), &RenderWorker::renderFrameReady, this,
          &Renderer::renderFrameUpdated);

  const auto isThreaded = true;
  if (isThreaded) {
    SPDLOG_WARN("Rendering is threaded! This is a beta feature!");
    // This is a beta feature that does not work on all systems!
    // Create the render thread ...
    mRenderThread = std::make_unique<QThread>();
    // ... and move the worker to the render thread ...
    mRenderWorker->moveToThread(mRenderThread.get());
    // ... and start the thread with highest priority
    mRenderThread->start(QThread::Priority::HighPriority);
  }
  // initialize will create the ROM
  emit initializeRenderer();
}

Renderer::~Renderer() {
  // should have stopped before (when MainWindow is destructed)
  stop();
}

void Renderer::start(QOpenGLContext* shareContext) {
  SPDLOG_INFO("Start Renderer..");
  // the desired format
  QSurfaceFormat format = QSurfaceFormat::defaultFormat();
  format.setSamples(8);
#ifdef NIMAGNA_DEBUG
  format.setOption(QSurfaceFormat::DebugContext);
#endif
  // Create OpenGL context
  // Note: destruction can happen in the render thread, thus, we do not keep a reference but just
  // pass it on to the render worker
  auto context = std::make_shared<QOpenGLContext>();
  context->setFormat(format);
  // share context with the main context
  context->setShareContext(shareContext);
  context->create();

  // create offscreen surface
  // Note: destruction must happen in this (main) thread again, thus, we keep the surface as shared
  // member
  mOffscreenSurface = std::make_shared<QOffscreenSurface>();
  mOffscreenSurface->create();
  mOffscreenSurface->setFormat(context->format());

  format = mOffscreenSurface->format();
  SPDLOG_INFO(
      "Offscreen format post set format: profile: {}, version: {}.{}, renderableType={}, "
      "samples={}",
      format.profile(), format.majorVersion(), format.minorVersion(), format.renderableType(),
      format.samples());

  const auto isThreaded = true;
  if (isThreaded && mRenderThread) {
    // ... and move the context and offscreen surface to the render thread ...
    mOffscreenSurface->moveToThread(mRenderThread.get());
    context->moveToThread(mRenderThread.get());
  }
  // and trigger the render worker to start. This must be done with a signal to have the execution
  // in the render thread. In a non-threaded environment, this is a direct connected signal.
  emit startRenderer(context, mOffscreenSurface);
}

std::shared_ptr<nimagna::RenderObjectManager> Renderer::renderObjectManager() const {
  if (!mRenderWorker) return nullptr;
  return mRenderWorker->renderObjectManager();
}

void Renderer::stop() {
  if (!mRenderWorker) {
    SPDLOG_INFO("Renderer already stopped");
    return;
  }
  if (mRenderWorker->isActive()) {
    // signal stop to the render worker
    // in non-threaded environment, this is executed directly
    SPDLOG_INFO("Asking renderer to stop...");
    emit stopRenderer();
  }
  if (mRenderThread && !mRenderThread->isFinished()) {
    // existence of the thread is sufficient to know that the current instance ran with threaded
    // rendering -> wait for the render thread to finish
    while (mRenderWorker->isActive()) {
      const auto kWaitTime = 10;
      mRenderThread->wait(QDeadlineTimer(kWaitTime));
    }
    assert(!mRenderWorker->isActive());
    SPDLOG_INFO("Quitting renderer thread...");
    mRenderThread->quit();
    if (const auto kWaitTime = 5000; mRenderThread->wait(QDeadlineTimer(kWaitTime))) {
      SPDLOG_INFO(" > Renderer thread quit gracefully.");
    }
    if (!mRenderThread->isFinished()) {
      SPDLOG_WARN(" > Renderer thread did not quit... -> terminating!");
      mRenderThread->terminate();
    }
    mRenderThread.reset();
  }
  assert(!mRenderWorker->isActive());
  // destroy the surface and worker
  mOffscreenSurface.reset();
  mRenderWorker.reset();
}

void Renderer::addImage(QString filename) {
  emit loadImage(filename);
}

}  // namespace nimagna
