#pragma once

#include <QtCore/QThread>
#include <QtCore/QTimer>

#include "Rendering/RenderObjectManager.h"
#include "Rendering/Rendering.h"

namespace nimagna {

// The render worker performs the rendering work in a separate thread launched and owned by the
// Renderer
class RenderWorker final : public QObject {
  Q_OBJECT
 public:
  RenderWorker();
  // neither copyable nor movable
  RenderWorker(const RenderWorker& other) = delete;
  RenderWorker& operator=(const RenderWorker& other) = delete;
  RenderWorker(RenderWorker&&) = delete;
  RenderWorker& operator=(RenderWorker&&) = delete;

  // The renderer worker owns the render object manager
  // it is accessible to get the current state and might live in the rendering thread
  // attention: can be nullptr!
  std::shared_ptr<RenderObjectManager> renderObjectManager() const;
  // once the renderer is started and until it is stopped, the render worker is active
  bool isActive() { return mIsActive; }

 public slots:
  // initialize
  void initializeRendering();
  // start the rendering: creates the render object manager and passes the given context and
  // surface, then starts the render timer
  void startRendering(std::shared_ptr<QOpenGLContext> context,
                      std::shared_ptr<QOffscreenSurface> surface);
  // stops the rendering and tears down the ROM
  void stopRendering();
  // loads an image as texture render object
  void loadImage(QString filename);

 signals:
  // signals a rendered frame to the consumer, e.g. the virtual camera
  void renderFrameReady();

 private slots:
  // rendering triggered by the timer
  void render();

 private:
  void createAndStartTimerIfNeeded();
  void udpateOutputFps();

  // timer to trigger rendering
  std::unique_ptr<QTimer> mTimer;
  // render object manager doing the rendering
  std::shared_ptr<RenderObjectManager> mRenderObjectManager;
  std::atomic_bool mIsActive = false;
};

// The exposed Renderer manages the render thread and triggers actions through the (queued) signals.
// In a threaded environment, the OpenGL context and offscreen surface must be created in the main
// (gui) thread and then moved to the rendering thread. Therefore, the Renderer handles this in the
// start() method.
class RENDERING_API Renderer final : public QObject {
  Q_OBJECT
 public:
  Renderer();
  // neither copyable nor movable
  Renderer(const Renderer& other) = delete;
  Renderer& operator=(const Renderer& other) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;
  ~Renderer();

  // Start the rendering: the OpenGL context from the main window/opengl widget will be shared
  // with the renderer's own OpenGL context which must be created in the main (gui) thread
  void start(QOpenGLContext* shareContext);
  // stop the rendering
  void stop();

  void addImage(QString filename);

  // access to the ROM
  std::shared_ptr<RenderObjectManager> renderObjectManager() const;

 signals:
  // initialize the render worker
  void initializeRenderer();
  // signal to start the renderer. passes the created context and offscreen surface to the renderer
  // worker
  void startRenderer(std::shared_ptr<QOpenGLContext> context,
                     std::shared_ptr<QOffscreenSurface> surface);
  // stop the renderer
  void stopRenderer();
  // signal that the the rendered frame was updated
  void renderFrameUpdated();

  void loadImage(QString filename);

 private:
  // The render worker performs the rendering
  std::unique_ptr<RenderWorker> mRenderWorker;
  // The offscreen surface is used to render into and pass the results to the main window/opengl
  // widget
  std::shared_ptr<QOffscreenSurface> mOffscreenSurface;
  // In threaded mode, the render worker runs the rendering thread
  std::unique_ptr<QThread> mRenderThread;
};
}  // namespace nimagna
