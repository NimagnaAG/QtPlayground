#include "pch.h"

#include "RenderObjectManagerViewModel.h"

#include "Rendering/RenderObject.h"

namespace nimagna {

RenderObjectManagerViewModel::RenderObjectManagerViewModel(std::shared_ptr<Renderer> renderer)
    : mRenderer(std::move(renderer)) {
  connect(mRenderer.get(), &Renderer::initialized, [this]() { initialize(); });
}

void RenderObjectManagerViewModel::initialize() {
  // is there no ROM? -> no good
  if (!mRenderer->renderObjectManager()) {
    SPDLOG_ERROR("No ROM in renderer!");
    return;
  }

  mRenderObjectManager = mRenderer->renderObjectManager();
  connect(mRenderObjectManager.get(), &RenderObjectManager::beginReset, this,
          &RenderObjectManagerViewModel::handleBeginReset);
  connect(mRenderObjectManager.get(), &RenderObjectManager::endReset, this,
          &RenderObjectManagerViewModel::handleEndReset);
  connect(mRenderObjectManager.get(), &RenderObjectManager::beginInsertRows, this,
          &RenderObjectManagerViewModel::handleBeginInsertRows);
  connect(mRenderObjectManager.get(), &RenderObjectManager::endInsertRows, this,
          &RenderObjectManagerViewModel::handleEndInsertRows);
}

int RenderObjectManagerViewModel::rowCount(const QModelIndex& /*parent*/) const {
  if (!mRenderObjectManager) {
    return 0;
  }
  return static_cast<int>(mRenderObjectManager->renderObjects().size());
}

int RenderObjectManagerViewModel::columnCount(const QModelIndex& /*parent*/) const {
  return kColumnCount;
}

QVariant RenderObjectManagerViewModel::data(const QModelIndex& index, int role) const {
  std::shared_ptr<RenderObject> object = mRenderObjectManager->renderObjects()[index.row()];
  if (object) {
    switch (role) {
      case Qt::DisplayRole:
      case Qt::EditRole: {
        switch (static_cast<TableColumns>(index.column())) {
          case TableColumns::kPositionX:
            return object->position().x();
          case TableColumns::kPositionY:
            return object->position().y();
          case TableColumns::kPositionZ:
            return object->position().z();
          case TableColumns::kScale:
            return object->scale();
          case TableColumns::kRotationX:
            return object->rotation().x();
          case TableColumns::kRotationY:
            return object->rotation().y();
          case TableColumns::kRotationZ:
            return object->rotation().z();
          default:
            return QVariant();
        }
        break;
      }
    }
  }
  return {};
}

QVariant RenderObjectManagerViewModel::headerData(int section, Qt::Orientation orientation,
                                                  int role /*= Qt::DisplayRole*/) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (static_cast<TableColumns>(section)) {
      case TableColumns::kPositionX:
        return "X";
      case TableColumns::kPositionY:
        return "Y";
      case TableColumns::kPositionZ:
        return "Z";
      case TableColumns::kScale:
        return "Scale";
      case TableColumns::kRotationX:
        return "Rx";
      case TableColumns::kRotationY:
        return "Ry";
      case TableColumns::kRotationZ:
        return "Rz";
      default:
        return QVariant();
    }
  }

  return QVariant();
}

bool RenderObjectManagerViewModel::setData(const QModelIndex& index, const QVariant& value,
                                           int role /*= Qt::EditRole*/) {
  std::shared_ptr<RenderObject> object = mRenderObjectManager->renderObjects()[index.row()];
  if (object) {
    auto position = object->position();
    auto rotation = object->rotation();
    switch (role) {
      case Qt::EditRole: {
        switch (static_cast<TableColumns>(index.column())) {
          case TableColumns::kPositionX: {
            // set x position
            position.setX(value.toFloat());
            object->setPosition(position);
            return true;
          }
          case TableColumns::kPositionY: {
            // set y position
            position.setY(value.toFloat());
            object->setPosition(position);
            return true;
          }
          case TableColumns::kPositionZ: {
            // set x position
            position.setZ(value.toFloat());
            object->setPosition(position);
            return true;
          }
          case TableColumns::kScale: {
            object->setScale(value.toFloat());
            return true;
          }
          case TableColumns::kRotationX: {
            // set x rotation
            rotation.setX(value.toFloat());
            object->setRotation(rotation);
            return true;
          }
          case TableColumns::kRotationY: {
            // set y rotation
            rotation.setY(value.toFloat());
            object->setRotation(rotation);
            return true;
          }
          case TableColumns::kRotationZ: {
            // set z rotation
            rotation.setZ(value.toFloat());
            object->setRotation(rotation);
            return true;
          }
          default:
            return false;
        }
        break;
      }
    }
  }
  return false;
}

Qt::ItemFlags RenderObjectManagerViewModel::flags(const QModelIndex& index) const {
  switch (static_cast<TableColumns>(index.column())) {
    case TableColumns::kPositionX:
    case TableColumns::kPositionY:
    case TableColumns::kPositionZ:
    case TableColumns::kScale:
    case TableColumns::kRotationX:
    case TableColumns::kRotationY:
    case TableColumns::kRotationZ:
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    default:
      // not editable
      break;
  }
  return QAbstractTableModel::flags(index);
}

void RenderObjectManagerViewModel::handleBeginReset() {
  beginResetModel();
}

void RenderObjectManagerViewModel::handleEndReset() {
  endResetModel();
}

void RenderObjectManagerViewModel::handleBeginInsertRows(int first, int last) {
  beginInsertRows(QModelIndex(), first, last);
}

void RenderObjectManagerViewModel::handleEndInsertRows() {
  endInsertRows();
}

}  // namespace nimagna
