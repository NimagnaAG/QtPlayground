// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing
// features. However, files listed here are ALL re-compiled if any one of them is updated between
// builds. Do not add files here that you will be updating frequently as this negates the
// performance advantage.

#pragma once

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
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

#include <spdlog/spdlog.h>

#include <QtCore/QElapsedTimer>
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
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>
#include "Logging.h"