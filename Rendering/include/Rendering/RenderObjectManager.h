#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QUuid>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtOpenGL/QOpenGLDebugLogger>
#include <QtOpenGL/QOpenGLFramebufferObject>

#include "Rendering/RenderData.h"
#include "Rendering/RenderObject.h"
#include "Rendering/Rendering.h"
#include "Rendering/TextureRenderObject.h"

namespace nimagna {

/*
 The RenderObjectManager is the core of the rendering system.

 It renders the render objects into an offscreen framebuffer object (FBO) in a separate thread and
 using its own context. This can be done using multisampling or not. All render objects are stored
 in a list and render itself into the FBO. The RenderObjectManager offers the offscreen
 framebuffer object as a texture to the OpenGL widget.

 The resolution of the offscreen framebuffer object is fixed to 1920x1080 pixels by default, but
 should be considered as potentially dynamic. The RenderObjectManager::onOutputSettingsChanged is
 resposible to update the FBO if the rendering resolution changes.

 Note: The rendering does not use depth by default. The ROM renders each object in the order they
 are added - potentially rendering over each other. If a render object needs depth, it must enable
 it itself and disable after rendering.

 The camera/world/object projection works as follows:
 - The RenderObjectManager uses the RenderData in mCurrentRenderData as the camera to world
 projection. This can be altered using the trackball and keyboard controls from the OpenGlWidget.
 - Each render object can have its own transformation matrix, defining the position, rotation, and
 scale of the object in the world coordinate system.
 - During rendering, the RenderObjectManager prepares each object with the projection matrix from
 mCurrentRenderData to each object such that the object gets the full model view projection matrix.
 - Then, the object renders itself into the framebuffer object.
 */
class RENDERING_API RenderObjectManager final : public QObject {
  Q_OBJECT

  friend class RenderWorker;

 public:
  // the supported render object types. Use with RenderObjectManager::addObject to add a render
  // object.
  enum class RenderObjectType { kTexture, kGltf, kGs, kGeoGs, kPly };

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

  // access the current camera to world information
  const std::shared_ptr<RenderData>& currentRenderData() const { return mCurrentRenderData; };
  bool isInitialized() const { return mIsInitialized; }
  // access the render frame buffer to be rendered as texture in the OpenGlWidget
  const std::unique_ptr<QOpenGLFramebufferObject>& renderFrameBuffer() const {
    return mRenderFramebuffer;
  }
  const TextureRenderObject::TextureTarget renderFrameBufferType() const {
    return mRenderFramebufferTarget;
  }
  const QSize& currentOutputResolution() const { return mCurrentOutputResolution; }

  // method to add a render object of a specific type. See also specific methods below
  void addObject(RenderObjectType type, const QString& filename);
  // the render objects
  RenderObjectList& renderObjects();
  // removes and deletes all render objects
  void clearRenderObjects();
  // active render objects are those that are currently rendered
  const RenderObjectList& activeRenderObjects() const;
  bool isActiveRenderObject(const std::shared_ptr<RenderObject> renderObject) const;
  // debugging
  void changeOpenGlDebugging(bool enabled);
  // the ordered list of all render objects
  RenderObjectList mRenderObjectsList;

  // methods to control rendering from UI
  void toggleRenderDepth(bool enabled);
  void resetViewMatrix();

 signals:
  void beginInsertRows(int first, int last);
  void endInsertRows();
  void beginReset();
  void endReset();

 protected slots:
  void onOutputSettingsChanged();
  void onOpenGlDebugMessage(const QOpenGLDebugMessage& debugMessage);

 private:
  // helper methods to create a texture render object
  std::shared_ptr<RenderObject> createTextureObject(const QString& filename);

  // pass the context to the render object manager and initialize
  void initialize(std::shared_ptr<QOpenGLContext> context,
                  std::shared_ptr<QOffscreenSurface> surface);
  // perform a rendering step
  bool render();
  void cleanUp();

  // make OpenGL context the current context
  bool tryMakeOpenGlContextCurrent(bool isCritical);

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
  QSize mCurrentOutputResolution = {1920, 1080};

  // the core application
  std::shared_ptr<RenderData> mCurrentRenderData;
};

}  // namespace nimagna
