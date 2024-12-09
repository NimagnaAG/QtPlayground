#include "Rendering/pch.h"

#include "Rendering/RenderData.h"

#include <QtCore/QJsonArray>
#include <numbers>

namespace nimagna {

RenderData::ShotFraming2D::ShotFraming2D(float left, float right, float bottom, float top)
    : mLeft(left), mRight(right), mBottom(bottom), mTop(top){};

RenderData::ShotFraming2D RenderData::ShotFraming2D::operator*(float timeFactor) const {
  return ShotFraming2D(mLeft * timeFactor, mRight * timeFactor, mBottom * timeFactor,
                       mTop * timeFactor);
}

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

RenderData::ShotFraming3D::ShotFraming3D(QVector3D position /*= QVector3D(0,0,2)*/,
                                         QVector3D lookAt /*= QVector3D(0,0,0)*/,
                                         float fieldOfView /*= 45.0f*/)
    : mPosition(position), mLookAtPoint(lookAt), mFieldOfViewAngle(fieldOfView) {}

RenderData::ShotFraming3D RenderData::ShotFraming3D::operator*(float timeFactor) const {
  return ShotFraming3D(mPosition * timeFactor, mLookAtPoint * timeFactor,
                       mFieldOfViewAngle * timeFactor);
}

RenderData::ShotFraming3D RenderData::ShotFraming3D::operator+(const ShotFraming3D& framing) const {
  return ShotFraming3D(mPosition + framing.mPosition, mLookAtPoint + framing.mLookAtPoint,
                       mFieldOfViewAngle + framing.mFieldOfViewAngle);
}

RenderData::ShotFraming3D RenderData::ShotFraming3D::operator-(const ShotFraming3D& framing) const {
  return ShotFraming3D(mPosition - framing.mPosition, mLookAtPoint - framing.mLookAtPoint,
                       mFieldOfViewAngle - framing.mFieldOfViewAngle);
}

RenderData::~RenderData() {
  disconnect();
}

RenderData::RenderData(RenderData&& other) noexcept {
  mRenderMode = std::move(other.mRenderMode);
  mShotFraming2D = std::move(other.mShotFraming2D);
  mShotFraming3D = std::move(other.mShotFraming3D);
}

RenderData::RenderData(const RenderData& other)
    : mShotFraming2D(other.mShotFraming2D),
      mShotFraming3D(other.mShotFraming3D),
      mRenderMode(other.mRenderMode) {
}

RenderData& RenderData::operator=(const RenderData& other) {
  if (this == &other) return *this;
  mRenderMode = other.mRenderMode;
  mShotFraming2D = other.mShotFraming2D;
  mShotFraming3D = other.mShotFraming3D;
  return *this;
}

RenderData& RenderData::operator=(RenderData&& other) noexcept {
  if (this == &other) return *this;
  mRenderMode = std::move(other.mRenderMode);
  mShotFraming2D = std::move(other.mShotFraming2D);
  mShotFraming3D = std::move(other.mShotFraming3D);
  return *this;
}

void RenderData::setRenderMode(RenderMode renderMode) {
  mRenderMode = renderMode;
}

void RenderData::setFraming2D(const ShotFraming2D& framing2D) {
  mShotFraming2D = framing2D;
}

void RenderData::setFraming3D(const ShotFraming3D& framing3D) {
  mShotFraming3D = framing3D;
}

QMatrix4x4 RenderData::projectionMatrix() const {
  QMatrix4x4 projM;
  if (is2D()) {
    // 2D projection
    projM.setToIdentity();
    const auto& framing = framing2D();
    projM.ortho(framing.left(), framing.right(), framing.bottom(), framing.top(),
                -100 /*nearPlane*/, 100 /*farPlane*/);
  } else {
    // 3D projection
    const auto& framing = framing3D();
    QMatrix4x4 view;
    const QVector3D upVector(0, 1, 0);
    view.lookAt(framing.position(), framing.lookAtPoint(), upVector);
    QMatrix4x4 proj;
    const float aspectRatio = 1.0f;
    const float nearPlane = 0.1f;
    const float farPlane = 0.1f;
    proj.perspective(framing.fieldOfViewAngle(), aspectRatio, nearPlane, farPlane);
    projM = proj * view;
  }
  return projM;
}

}  // namespace nimagna
