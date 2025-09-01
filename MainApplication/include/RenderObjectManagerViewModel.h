#pragma once
#include <QtCore/QAbstractTableModel>
#include <memory>

#include "Rendering/Renderer.h"

namespace nimagna {

// the model/view model of the render object manager
// implementing the QAbstractTableModel
// according to https://doc.qt.io/qt-5/qabstracttablemodel.html
// and https://doc.qt.io/qt-5/modelview.html
class RenderObjectManagerViewModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  explicit RenderObjectManagerViewModel(std::shared_ptr<Renderer> renderer);
  // not copyable
  RenderObjectManagerViewModel(const RenderObjectManagerViewModel& other) = delete;
  RenderObjectManagerViewModel& operator=(const RenderObjectManagerViewModel& other) = delete;
  // movable
  RenderObjectManagerViewModel(RenderObjectManagerViewModel&&) = delete;
  RenderObjectManagerViewModel& operator=(RenderObjectManagerViewModel&&) = delete;

  void initialize();

  [[nodiscard]] int rowCount(const QModelIndex& /*parent*/) const override;
  [[nodiscard]] int columnCount(const QModelIndex& /*parent*/) const override;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
  [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                    int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

 private slots:
  void handleBeginReset();
  void handleEndReset();
  void handleBeginInsertRows(int first, int last);
  void handleEndInsertRows();

 private:
  enum class TableColumns {
    kPositionX = 0,
    kPositionY,
    kPositionZ,
    kScaleX,
    kScaleY,
    kScaleZ,
    kRotationX,
    kRotationY,
    kRotationZ,
  };
  const int kColumnCount = 7;

  std::shared_ptr<Renderer> mRenderer;
  std::shared_ptr<RenderObjectManager> mRenderObjectManager;
};

}  // namespace nimagna
