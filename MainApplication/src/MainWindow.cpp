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
    mRenderer->loadImage(fileName);
  }
}

void MainWindow::onOpenGlWidgetInitialized() const {
  mRenderer->start(mUI.openGLWidget->context());
  mUI.openGLWidget->update();
}

void MainWindow::connectSignalsAndSlots() {
  // OpenGL Widget: initialized/trackball disabled
  connect(mUI.openGLWidget, &OpenGlWidget::initialized, this,
          &MainWindow::onOpenGlWidgetInitialized);
}

}  // namespace nimagna
