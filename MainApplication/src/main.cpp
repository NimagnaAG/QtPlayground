#include "pch.h"

#include <QtCore/QDir>
#include <QtCore/QResource>

#include "LoggerMainApplication.h"
#include "MainWindow.h"

int main(int argc, char* argv[]) {
  using namespace nimagna;

  // OpenGL surface preset
  QSurfaceFormat format;
  format.setVersion(4, 0);
  format.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(format);

  // To render the output preview also in a separate window, the OpenGLContexts must be shared
  // explicitly
  QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

// Don't show the action icons in the menus, compliant with macOS
#if NIMAGNA_MACOS
  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, true);
#endif

  // the main application
  QApplication app(argc, argv);
  app.thread()->setPriority(QThread::Priority::HighPriority);

  // Set working directory to the executable directory
  QDir::setCurrent(QCoreApplication::applicationDirPath());
  // System configuration with folders
  QApplication::setOrganizationName("Nimagna");
  QApplication::setApplicationName("Test");

  // configure logging
  SPDLOG_INFO("Configuring Logging...");
  LoggerMainApplication::configureAll(argc, argv);
  SPDLOG_INFO("... Done configuring Logging");

  // loading resources
  SPDLOG_INFO("Loading resources...");
  if (auto resourceFile = QFileInfo(QCoreApplication::applicationDirPath() + QDir::separator() +
                                    "MainApplication.rcc");
      resourceFile.exists()) {
    if (!QResource::registerResource(resourceFile.absoluteFilePath())) {
      SPDLOG_CRITICAL("> Failed to load resources! {}", resourceFile.absoluteFilePath());
    }
  } else {
    SPDLOG_CRITICAL("> Resource file does not exist!");
  }
  SPDLOG_INFO("> Done");

  // create and run MainApplication
  SPDLOG_INFO("Creating MainWindow...");
  auto mainWindow = std::make_unique<MainWindow>();
  SPDLOG_INFO("... Done creating MainWindow");
  // show the window
  SPDLOG_INFO("Showing MainWindow...");
  mainWindow->show();
  SPDLOG_INFO("Executing main application...");
  const int result = QApplication::exec();
  SPDLOG_INFO("Main application ended...");

  // post-run clean up
  mainWindow.reset();  // destructs MainWindow
  SPDLOG_INFO("MainApplication END");
  LoggerMainApplication::flushAll();
  return result;
}

#if NIMAGNA_WINDOWS
// Dual GPU laptops need these to select the dedicated GPU
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}
extern "C" {
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
