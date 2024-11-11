#include "Rendering/pch.h"

#include "Rendering/RenderObject.h"


namespace nimagna {

RenderObject::RenderObject() : mIsInitialized(false), mLayer(0) {}

void RenderObject::initialize() {
  mIsInitialized = true;
}

void RenderObject::prepare(const QMatrix4x4& vp) {
  mViewProjectionMatrix = vp;
}

float RenderObject::alpha() const {
  return mFallbackAlpha;
}

void RenderObject::setFallbackAlpha(float alphaValue) {
  mFallbackAlpha = std::clamp(alphaValue, 0.0f, 1.0f);
}

const QMatrix4x4& RenderObject::getModelMatrix() const {
  return mModelMatrix;
}

void RenderObject::setDisplayName(const QString& displayName) {
  SPDLOG_INFO("Set display name: {}", displayName.toStdString());
  mDisplayName = displayName;
  emit propertiesChanged();
}

QString RenderObject::getDisplayName() const {
  return mDisplayName;
}

bool RenderObject::isInitialized() const {
  return mIsInitialized;
}

void RenderObject::setLayer(int layer) {
  mLayer = layer;
  emit propertiesChanged();
}

int RenderObject::layer() const {
  return mLayer;
}

}  // namespace nimagna
