#pragma once

#include <QtCore/QSize>
#include <QtGui/QMatrix4x4>
#include <vector>

#include "Rendering/Rendering.h"

namespace nimagna {

class RENDERING_API RenderData {
 public:
  enum class RenderMode { Render2D, Render3D };

  // the shot framing in 2D mode
  class RENDERING_API ShotFraming2D {
   public:
    explicit ShotFraming2D(float left = -1., float right = 1., float bottom = -1., float top = 1);
    // copyable
    ShotFraming2D(const ShotFraming2D&) = default;
    ShotFraming2D& operator=(const ShotFraming2D&) = default;
    // movable
    ShotFraming2D(ShotFraming2D&& other) = default;
    ShotFraming2D& operator=(ShotFraming2D&& other) = default;
    // operators
    ShotFraming2D operator+(const ShotFraming2D& framing) const;
    ShotFraming2D operator-(const ShotFraming2D& framing) const;
    bool operator==(const ShotFraming2D& framing) const = default;
    float left() const { return mLeft; };
    void setLeft(float leftPosition) { mLeft = leftPosition; };
    float right() const { return mRight; };
    void setRight(float rightPosition) { mRight = rightPosition; };
    float bottom() const { return mBottom; };
    void setBottom(float bottomPosition) { mBottom = bottomPosition; };
    float top() const { return mTop; };
    void setTop(float topPosition) { mTop = topPosition; };

   private:
    float mLeft;
    float mRight;
    float mBottom;
    float mTop;
  };

  // the shot framing in 3D mode
  class RENDERING_API ShotFraming3D {
   public:
    explicit ShotFraming3D(float fieldOfView = 90.f);
    // copyable
    ShotFraming3D(const ShotFraming3D&) = default;
    ShotFraming3D& operator=(const ShotFraming3D&) = default;
    // movable
    ShotFraming3D(ShotFraming3D&& other) = default;
    ShotFraming3D& operator=(ShotFraming3D&& other) = default;
    // operators
    ShotFraming3D operator*(float timeFactor) const;
    ShotFraming3D operator+(const ShotFraming3D& framing) const;
    ShotFraming3D operator-(const ShotFraming3D& framing) const;
    bool operator==(const ShotFraming3D& framing) const = default;

    float fieldOfViewAngle() const { return mFieldOfViewAngle; }
    void setFieldOfViewAngle(float fieldOfViewAngle);

    explicit ShotFraming3D(const QJsonObject& from);
    explicit operator QJsonObject() const;

   private:
    float mFieldOfViewAngle;
  };

  RenderData() = default;
  // copyable
  RenderData(const RenderData& other);
  RenderData& operator=(const RenderData& other);
  // and movable
  RenderData(RenderData&& other) noexcept;
  RenderData& operator=(RenderData&& other) noexcept;

  // rendering/animation
  bool is2D() const { return mRenderMode == RenderMode::Render2D; };
  bool is3D() const { return mRenderMode == RenderMode::Render3D; }
  RenderMode renderMode() const { return mRenderMode; }
  void setRenderMode(RenderMode renderMode);

  const ShotFraming2D& framing2D() const { return mShotFraming2D; };
  const ShotFraming3D& framing3D() const { return mShotFraming3D; };

  void setFraming2D(const ShotFraming2D& framing2D);
  void setFraming3D(const ShotFraming3D& framing3D);

  void setViewMatrix(const QMatrix4x4& viewMatrix) { mViewMatrix = viewMatrix; }
  const QMatrix4x4& viewMatrix() const { return mViewMatrix; }
  QMatrix4x4 projectionMatrix() const;
  float fieldOfViewAngle() { return mShotFraming3D.fieldOfViewAngle(); }
  void setViewport(const QSize& viewport) {
    mViewport = viewport;
    updateProjectionMatrix();
  };
  QSize viewport() const { return mViewport; }

 protected:
  ShotFraming2D mShotFraming2D;
  ShotFraming3D mShotFraming3D;
  QSize mViewport;
  QMatrix4x4 mViewMatrix;
  void updateProjectionMatrix();
  QMatrix4x4 mProjectionMatrix;
  const float mNearPlane = 0.2f;
  const float mFarPlane = 200.f;

 private:
  RenderMode mRenderMode = RenderMode::Render2D;
};

}  // namespace nimagna
