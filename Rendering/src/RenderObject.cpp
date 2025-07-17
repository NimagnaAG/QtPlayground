#include "Rendering/pch.h"

#include "Rendering/RenderObject.h"

namespace nimagna {

RenderObject::RenderObject() : mIsInitialized(false), mLayer(0) {}

void RenderObject::initialize() {
  mIsInitialized = true;
}

void RenderObject::setModelMatrix(const QMatrix4x4& modelMatrix) {
  mModelMatrix = modelMatrix;
  emit propertiesChanged();
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
