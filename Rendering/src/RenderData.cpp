#include "Rendering/pch.h"

#include "Rendering/RenderData.h"

namespace nimagna {

RenderData::ShotFraming2D::ShotFraming2D(float left, float right, float bottom, float top)
    : mLeft(left), mRight(right), mBottom(bottom), mTop(top) {};

RenderData::ShotFraming2D RenderData::ShotFraming2D::operator+(
    const RenderData::ShotFraming2D& otherFraming) const {
  return ShotFraming2D(mLeft + otherFraming.mLeft, mRight + otherFraming.mRight,
                       mBottom + otherFraming.mBottom, mTop + otherFraming.mTop);
}

RenderData::ShotFraming2D RenderData::ShotFraming2D::operator-(
    const RenderData::ShotFraming2D& otherFraming) const {
  return ShotFraming2D(mLeft - otherFraming.mLeft, mRight - otherFraming.mRight,
                       mBottom - otherFraming.mBottom, mTop - otherFraming.mTop);
}

///////////////////////////////////////////////////////////////////////////////////////

RenderData::ShotFraming3D::ShotFraming3D(float fieldOfView /*= 45.0f*/)
    : mFieldOfViewAngle(fieldOfView) {}

RenderData::ShotFraming3D RenderData::ShotFraming3D::operator+(const ShotFraming3D& framing) const {
  return ShotFraming3D(mFieldOfViewAngle + framing.mFieldOfViewAngle);
}

RenderData::ShotFraming3D RenderData::ShotFraming3D::operator-(const ShotFraming3D& framing) const {
  return ShotFraming3D(mFieldOfViewAngle - framing.mFieldOfViewAngle);
}

RenderData::RenderData(RenderData&& other) noexcept {
  mRenderMode = std::move(other.mRenderMode);
  mShotFraming2D = std::move(other.mShotFraming2D);
  mShotFraming3D = std::move(other.mShotFraming3D);
  updateProjectionMatrix();
}

RenderData::RenderData(const RenderData& other)
    : mShotFraming2D(other.mShotFraming2D),
      mShotFraming3D(other.mShotFraming3D),
      mRenderMode(other.mRenderMode) {
  updateProjectionMatrix();
}

RenderData& RenderData::operator=(const RenderData& other) {
  if (this == &other) return *this;
  mRenderMode = other.mRenderMode;
  mShotFraming2D = other.mShotFraming2D;
  mShotFraming3D = other.mShotFraming3D;
  updateProjectionMatrix();
  return *this;
}

RenderData& RenderData::operator=(RenderData&& other) noexcept {
  if (this == &other) return *this;
  mRenderMode = std::move(other.mRenderMode);
  mShotFraming2D = std::move(other.mShotFraming2D);
  mShotFraming3D = std::move(other.mShotFraming3D);
  updateProjectionMatrix();
  return *this;
}

void RenderData::setRenderMode(RenderMode renderMode) {
  mRenderMode = renderMode;
  updateProjectionMatrix();
}

void RenderData::setFraming2D(const ShotFraming2D& framing2D) {
  mShotFraming2D = framing2D;
  updateProjectionMatrix();
}

void RenderData::setFraming3D(const ShotFraming3D& framing3D) {
  mShotFraming3D = framing3D;
  updateProjectionMatrix();
}

QMatrix4x4 RenderData::projectionMatrix() const {
  return mProjectionMatrix;
}

void RenderData::updateProjectionMatrix() {
  mProjectionMatrix = QMatrix4x4();
  const float aspectRatio = viewport().width() / static_cast<float>(viewport().height());
  if (is2D()) {
    // 2D projection
    const auto& framing = framing2D();
    if (aspectRatio > 1.f) {
      mProjectionMatrix.ortho(framing.left() / aspectRatio, framing.right() / aspectRatio,
                              framing.bottom(), framing.top(), -100 /*nearPlane*/,
                              100 /*farPlane*/);
    } else {
      mProjectionMatrix.ortho(framing.left(), framing.right(), framing.bottom() * aspectRatio,
                              framing.top() * aspectRatio, -100 /*nearPlane*/, 100 /*farPlane*/);
    }
  } else {
    // 3D projection
    const auto& framing = framing3D();
    mProjectionMatrix.perspective(framing.fieldOfViewAngle(), aspectRatio, mNearPlane, mFarPlane);
  }
}

void RenderData::ShotFraming3D::setFieldOfViewAngle(float fieldOfViewAngle) {
  mFieldOfViewAngle = fieldOfViewAngle;
}

}  // namespace nimagna
