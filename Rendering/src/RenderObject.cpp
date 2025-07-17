#include "Rendering/pch.h"

#include "Rendering/RenderObject.h"

namespace nimagna {

RenderObject::RenderObject() : mIsInitialized(false), mLayer(0) {}

void RenderObject::initialize() {
  mIsInitialized = true;
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

void RenderObject::updateModelMatrix() {
  QMatrix4x4 modelMatrix;
  mModelMatrix.setToIdentity();
  modelMatrix.translate(mPosition);
  modelMatrix.rotate(mRotation.x(), {1.0, 0.0, 0.0});
  modelMatrix.rotate(mRotation.y(), {0.0, 1.0, 0.0});
  modelMatrix.rotate(mRotation.z(), {0.0, 0.0, 1.0});
  modelMatrix.scale(mScale);
  mModelMatrix = modelMatrix;
  emit propertiesChanged();
}

}  // namespace nimagna
