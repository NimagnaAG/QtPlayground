#include "pch.h"

#include "MainWindow.h"

#include <QtCore/QJsonObject>
#include <QtGui/QDesktopServices>
#include <QtGui/QShortcut>
#include <QtWidgets/QMessageBox>

namespace nimagna {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  mUI.setupUi(this);

  // create renderer
  mRenderer = std::make_shared<Renderer>();
  mUI.openGLWidget->setRenderer(mRenderer);
  connectSignalsAndSlots();

  // create view model for the render object manager
  mRenderObjectManagerViewModel = std::make_unique<RenderObjectManagerViewModel>(mRenderer);
  mUI.tableView->setModel(mRenderObjectManagerViewModel.get());
}

MainWindow::~MainWindow() {
  if (mRenderer) {
    mRenderer->stop();
  }
}

void MainWindow::on_actionLoad_triggered() {
  SPDLOG_INFO("User action: load image");
  const QString fileName =
      QFileDialog::getOpenFileName(this, tr("Open Show"), "", tr("Image Files (*.png;*.jpg)"));
  if (!fileName.isNull()) {
    // not canceled
    mRenderer->addObject(RenderObjectManager::RenderObjectType::kTexture, fileName);
  }
}

void MainWindow::on_test_triggered() {
  SPDLOG_INFO("User action: test");
  const QString fileName =
      QFileDialog::getOpenFileName(this, tr("Open Show"), "", tr("GLTF (*.gltf;*.png)"));
  SPDLOG_INFO("Filename action: " + fileName);
  if (!fileName.isNull()) {
    // not canceled
    mRenderer->addObject(RenderObjectManager::RenderObjectType::kGltf, fileName);
  }
}
void MainWindow::on_gsload_triggered() {
  SPDLOG_INFO("User action: Load Gaussian Splat Video");
  const QString fileName = QFileDialog::getOpenFileName(this, tr("Open Show"), "",
                                                        tr("V-splat (*.vsplat;*.splat;*.ply)"));
  SPDLOG_INFO("Filename action: " + fileName);
  if (!fileName.isNull()) {
    // not canceled
    mRenderer->addObject(RenderObjectManager::RenderObjectType::kGs, fileName);
  }
}
void MainWindow::on_plyload_triggered() {
  SPDLOG_INFO("User action: Load PLY");
  const QString fileName =
      QFileDialog::getOpenFileName(this, tr("Open Show"), "", tr("PLY (*.ply)"));
  SPDLOG_INFO("Filename action: " + fileName);
  if (!fileName.isNull()) {
    // not canceled
    mRenderer->addObject(RenderObjectManager::RenderObjectType::kPly, fileName);
  }
}

void MainWindow::on_actionDepth_toggled(bool enabled) {
  mRenderer->renderObjectManager()->toggleRenderDepth(enabled);
}

void MainWindow::on_clearPushButton_clicked() {
  mRenderer->clear();
  mUI.actionDepth->setChecked(false);
}

void MainWindow::on_resetViewPushButton_clicked() {
  mRenderer->renderObjectManager()->resetViewMatrix();
}

void MainWindow::onOpenGlWidgetInitialized() const {
  mRenderer->start(mUI.openGLWidget->context());
  mUI.openGLWidget->update();
  mUI.actionDepth->setChecked(false);
  SPDLOG_INFO("ainWindow::onOpenGlWidgetInitialized() ");
}

void MainWindow::connectSignalsAndSlots() {
  // OpenGL Widget: initialized/trackball disabled
  connect(mUI.openGLWidget, &OpenGlWidget::initialized, this,
          &MainWindow::onOpenGlWidgetInitialized);
}

}  // namespace nimagna
