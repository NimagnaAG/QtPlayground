
#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
  #include <spdlog/sinks/msvc_sink.h>
  // Windows Header Files
  #include <windows.h>
#endif

#ifndef SPDLOG_ACTIVE_LEVEL
  #ifdef NIMAGNA_RELEASE
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
  #else
    #define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
  #endif  // !NIMAGNA_RELEASE
#endif    // !SPDLOG_ACTIVE_LEVEL

#include <spdlog/cfg/argv.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSettings>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QMatrix4x4>
#include <QtGui/QOffscreenSurface>
#include <QtGui/QOpenGLContext>
#include <QtGui/QQuaternion>
#include <QtGui/QVector3D>
#include <QtMultimedia/QCamera>
#include <QtMultimedia/QVideoFrame>
#include <QtOpenGL/QOpenGLBuffer>
#include <QtOpenGL/QOpenGLFramebufferObject>
#include <QtOpenGL/QOpenGLFunctions_4_0_Core>
#include <QtOpenGL/QOpenGLShaderProgram>
#include <QtOpenGL/QOpenGLTexture>
#include <QtOpenGL/QOpenGLVertexArrayObject>
#include <QtOpenGL/QOpenGLWindow>
#include <QtWidgets/QFileDialog>
#include <iostream>
#include <list>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "Rendering/Logging.h"
