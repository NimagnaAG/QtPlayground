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


const QMatrix4x4& RenderObject::getModelMatrix() const {
  return mModelMatrix;
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
