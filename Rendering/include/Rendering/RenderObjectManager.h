#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QUuid>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLDebugLogger>
#include <QtOpenGL/QOpenGLFramebufferObject>

#include "Rendering/RenderObject.h"
#include "Rendering/RenderData.h"
#include "Rendering/Rendering.h"
#include "Rendering/TextureRenderObject.h"

namespace nimagna {

// the RenderObjectManager is a data structure holding RenderObjects
class RENDERING_API RenderObjectManager final : public QObject {
  Q_OBJECT

  friend class RenderWorker;

 public:
  // not copyable but movable
  RenderObjectManager();
  RenderObjectManager(const RenderObjectManager& other) = delete;
  RenderObjectManager& operator=(const RenderObjectManager& other) = delete;
  RenderObjectManager(RenderObjectManager&&) = delete;
  RenderObjectManager& operator=(RenderObjectManager&&) = delete;
  ~RenderObjectManager();

  // a list of all objects
  using RenderObjectList = std::vector<std::shared_ptr<RenderObject>>;
  using FrameSourceUuidToRenderObjectMap = std::map<QUuid, std::shared_ptr<RenderObject>>;

  const std::shared_ptr<RenderData>& currentRenderData() const { return mCurrentRenderData; };
  bool isInitialized() const { return mIsInitialized; }
  const std::unique_ptr<QOpenGLFramebufferObject>& renderFrameBuffer() const {
    return mRenderFramebuffer;
  }
  const TextureRenderObject::TextureTarget renderFrameBufferType() const {
    return mRenderFramebufferTarget;
  }

  void addTextureObject(const QString& filename);

  const RenderObjectList& renderObjects() const;
  const RenderObjectList& activeRenderObjects() const;
  bool isActiveRenderObject(const std::shared_ptr<RenderObject> renderObject) const;
  void changeOpenGlDebugging(bool enabled);

 private:
  // pass the context to the render object manager and initialize
  bool render();
  void initialize(std::shared_ptr<QOpenGLContext> context,
                  std::shared_ptr<QOffscreenSurface> surface);
  void cleanUp();

 protected slots:
  void onOutputSettingsChanged();
  void onOpenGlDebugMessage(const QOpenGLDebugMessage& debugMessage);

 private:
  // make OpenGL context the current context
  bool tryMakeOpenGlContextCurrent(bool isCritical);

  // removes and deletes all render objects
  void clearRenderObjects();
  // access render objects
  int renderObjectListCount() const;
  int getRenderObjectRowIndex(const std::shared_ptr<RenderObject>& object) const;

  // the render object manager must wait for the OpenGL context in `initialize` and can be used only
  // afterwards
  bool mIsInitialized = false;

  // the offscreen rendering context, surface, and framebuffer object
  // Note: The offscreen surface needs to be created in the main thread.
  std::shared_ptr<QOpenGLContext> mContext;
  std::shared_ptr<QOffscreenSurface> mOffscreenSurface;
  std::unique_ptr<QOpenGLDebugLogger> mDebugLogger;
  std::map<GLuint, int> mDebugMessageIdCounter;

  std::unique_ptr<QOpenGLFramebufferObject> mRenderFramebuffer;
  const TextureRenderObject::TextureTarget mRenderFramebufferTarget =
      TextureRenderObject::kDefaultTextureTarget;
  std::unique_ptr<QOpenGLFramebufferObject> mMultisampleFramebuffer;
  QSize mCurrentOutputResolution = {};

  // the ordered list of all render objects
  RenderObjectList mRenderObjectsList;

  // the core application
  std::shared_ptr<RenderData> mCurrentRenderData;

};

}  // namespace nimagna
