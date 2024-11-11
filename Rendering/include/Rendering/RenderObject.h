#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QMatrix4x4>
#include <QtGui/QQuaternion>
#include <algorithm>  // std::clamp

#include "Rendering/Rendering.h"

namespace nimagna {

/* The base class for all render objects (RO)
 *
 * Render objects relate 1:1 to a ShotComponent and are managed by the RenderObjectManager (ROM).
 * First, create the object, then add it to the ROM before setting content and starting.
 *
 * Current inheritance hierarchy is:
 *	RenderObject: basics like position, scale, etc
 *	-> TextureRenderObject: texture rendering (all OpenGL code)
 *	  -> FrameSourceRenderObject: use a frame source as texture source)
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

  // draw the object. OpenGL context is active.
  virtual void draw() = 0;
  float alpha() const;
  void setFallbackAlpha(float alphaValue);
  // get the model matrix
  const QMatrix4x4& getModelMatrix() const;
  // prepare for rendering
  void prepare(const QMatrix4x4& vp);

  // the display name
  void setDisplayName(const QString& displayName);
  // get the display name
  QString getDisplayName() const;
  // check if initialized
  bool isInitialized() const;
  bool readyForRendering() const { return mIsReadyForRendering; }
  void setLayer(int layer);
  int layer() const;

 signals:
  void propertiesChanged();

 protected:
  // the view/projection matrix
  QMatrix4x4 mViewProjectionMatrix;
  // the model matrix
  QMatrix4x4 mModelMatrix;

 protected:
  // flag indicating if that render object is ready for rendering
  bool mIsReadyForRendering = true;

 private:
  // the display name
  QString mDisplayName;
  // flag indicating if that render object is initialized
  bool mIsInitialized;
  // the layer is a volatile member used
  int mLayer;
  // alpha value being used if shot component is not available
  float mFallbackAlpha = 1.0f;
  // allow updates flag
  bool mAllowUpdates = true;

  QString mResourceIdentifier;
};

}  // namespace nimagna
