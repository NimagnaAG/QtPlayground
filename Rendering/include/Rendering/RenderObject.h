#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QMatrix4x4>
#include <QtGui/QQuaternion>
#include <algorithm>  // std::clamp

#include "Rendering/RenderData.h"
#include "Rendering/Rendering.h"

namespace nimagna {

/** The base class for all render objects (RO)

  Render objects are managed by the RenderObjectManager (ROM) and render themselfs into the ROM's
  framebuffer object.

  Upon initialization, the render object is registered at the ROM and can be rendered. When
  overwriting the initialize method, the base class' initialize method must be called to ensure full
  initialization.

  During rendering, the ROM passes the render object the current camera view and projection
  matrices. This allows a render object to apply its own model matrix (if defined) on top to get the
  full model-view-projection matrix and render itself into the framebuffer object.
 */
class RENDERING_API RenderObject : public QObject {
  Q_OBJECT

 public:
  // the render object needs the OpenGL context
  RenderObject();
  // not copyable or movable
  RenderObject(const RenderObject& other) = delete;
  RenderObject& operator=(const RenderObject& other) = delete;
  RenderObject(RenderObject&&) = delete;
  RenderObject& operator=(RenderObject&&) = delete;

  // initialize the object. if overwritten, must call the base class' initialize method!
  // the initialize method is called upon registration at the RenderObjectManager.
  virtual void initialize();

  // set/get connected and active flag
  bool allowUpdates() const { return mAllowUpdates; }
  void setAllowUpdates(bool allow) { mAllowUpdates = allow; }

  // Overwrite the following methods to implement a derived class:
  // Draws the ob into the framebuffer object. This must ensure that the object sets up the OpenGL
  // state such that it can render itself. The object cannot assume that the OpenGL state is
  // preserved between two draw calls.
  virtual void draw(const std::shared_ptr<RenderData> renderData) = 0;
  // get and set the model matrix
  const QMatrix4x4& modelMatrix() const { return mModelMatrix; }

  const QVector3D& position() const { return mPosition; }
  void setPosition(const QVector3D& position) {
    mPosition = position;
    updateModelMatrix();
    emit propertiesChanged();
  }
  const QVector3D& rotation() const { return mRotation; }
  void setRotation(const QVector3D& rotationAngles) {
    mRotation = rotationAngles;
    updateModelMatrix();
    emit propertiesChanged();
  }
  const QVector3D& scale() const { return mScale; }
  void setScale(const QVector3D& scale) {
    mScale = {
        std::clamp(scale.x(), 0.01f, 100.0f), std::clamp(scale.y(), 0.01f, 100.0f),
        std::clamp(scale.z(), 0.01f, 100.0f)};  // prevent scale from being too small or too large
    updateModelMatrix();
    emit propertiesChanged();
  }
  // check if initialized
  bool isInitialized() const;
  bool readyForRendering() const { return mIsReadyForRendering; }
  void setLayer(int layer);
  int layer() const;

 signals:
  void propertiesChanged();

 protected:
  // flag indicating if that render object is ready for rendering
  bool mIsReadyForRendering = true;

 private:
  QVector3D mPosition = {0.0f, 0.0f, 0.0f};
  QVector3D mRotation = {0.0f, 0.0f, 0.0f};
  QVector3D mScale = {1.0f, 1.0f, 1.0f};
  // the object's own model matrix defines the position, rotation, and scale of the object in the
  // world coordinate system.
  QMatrix4x4 mModelMatrix;
  void updateModelMatrix();
  // flag indicating if that render object is initialized
  bool mIsInitialized;
  // the layer is a volatile member used to sort render objects in the rendering pipeline.
  int mLayer;
  // allow updates flag
  bool mAllowUpdates = true;
};

}  // namespace nimagna
