#pragma once

#include <QtCore/QFileSystemWatcher>
#include <QtCore/QThread>

#include "RenderObjectManagerViewModel.h"
#include "Rendering/RenderObjectManager.h"
#include "Rendering/Renderer.h"
#include "ui_MainWindow.h"

namespace nimagna {

/*
 * Main window
 */
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = Q_NULLPTR);
  // neither movable nor copyable
  MainWindow(const MainWindow& other) = delete;
  MainWindow& operator=(const MainWindow& other) = delete;
  MainWindow(MainWindow&&) = delete;
  MainWindow& operator=(MainWindow&&) = delete;

  virtual ~MainWindow();

 private slots:
  // --- Menu actions  ---
  // File menu
  void on_actionLoad_triggered();
  void on_test_triggered();
  void on_gsload_triggered();
  void on_plyload_triggered();
  // button actions
  void on_clearPushButton_clicked();
  void on_resetViewPushButton_clicked();
  // --- Callback from OpenGL window once initialized
  void onOpenGlWidgetInitialized() const;

 private:
  void connectSignalsAndSlots();

  Ui::MainWindow mUI;
  std::shared_ptr<Renderer> mRenderer;
  std::unique_ptr<RenderObjectManagerViewModel> mRenderObjectManagerViewModel;
};

}  // namespace nimagna
