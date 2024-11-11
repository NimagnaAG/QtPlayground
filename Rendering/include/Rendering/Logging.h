#pragma once

#include <spdlog/fmt/bundled/format.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/dist_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <QStringList>

#include "Rendering/Rendering.h"

/// <summary>
/// This file implements
/// - The logger info: Global settings on log location, log folder management, level, patterns, etc.
/// - The IMPLEMENT_LOGGER macro being used in all DLLs to implement the DLL specific logger
/// instances
/// - The logger instance of Base
/// </summary>

namespace nimagna {
class RENDERING_API LoggerInfo {
 public:
  static std::string logfileLocation();
  /* spdlog log levels:
    off = 6, critical = 5, err = 4, warn = 3, info = 2, debug = 1, trace = 0
  */
  static spdlog::level::level_enum logLevel();
  static std::string defaultLogPattern();

 private:
  static std::string generateLogfilePath();
  static inline spdlog::level::level_enum mLogLevel =
      spdlog::level::info;  // set initial 'info' log level
};

// The FunctionNameFormatter prints the function name without "nimagna::"
class RENDERING_API FunctionNameFormatter : public spdlog::custom_flag_formatter {
 public:
  void format(const spdlog::details::log_msg &message, const std::tm &,
              spdlog::memory_buf_t &dest) override;

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<FunctionNameFormatter>();
  }
};

}  // namespace nimagna

#define IMPLEMENT_LOGGER(Project, ApiMacro)                                           \
  namespace nimagna {                                                                 \
  class ApiMacro Logger##Project {                                                    \
   public:                                                                            \
    static void configure(std::shared_ptr<spdlog::sinks::dist_sink_st> sink) {        \
      auto logger = std::make_shared<spdlog::logger>("NimagnaLogger" #Project, sink); \
      defaultInitialization(logger);                                                  \
    };                                                                                \
    static void setPattern(const std::string &pattern) {                              \
      auto formatter = std::make_unique<spdlog::pattern_formatter>();                 \
      /* uses the special formatter for function names using the ~ pattern */         \
      formatter->add_flag<FunctionNameFormatter>('~').set_pattern(pattern);           \
      spdlog::set_formatter(std::move(formatter));                                    \
    };                                                                                \
    static void setLogLevel(spdlog::level::level_enum level) {                        \
      spdlog::set_level(level);                                                       \
    };                                                                                \
                                                                                      \
   private:                                                                           \
    static void defaultInitialization(std::shared_ptr<spdlog::logger> logger) {       \
      /* by default spdlog creates a default global logger(to stdout, colored and     \
       * multithreaded). This overwrites the default one with the custom logger */    \
      spdlog::set_default_logger(logger);                                             \
      setPattern(LoggerInfo::defaultLogPattern());                                    \
      setLogLevel(LoggerInfo::logLevel());                                            \
      spdlog::flush_on(spdlog::level::err);                                           \
    };                                                                                \
  };                                                                                  \
  }  // namespace nimagna

IMPLEMENT_LOGGER(Rendering, RENDERING_API);

/// <summary>
/// Formatter extensions for logging special types
/// </summary>

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtCore/QUuid>
#include <QtGui/QVector3D>

template <>
struct fmt::formatter<QString> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

  template <typename Context>
  auto format(const QString &s, Context &ctx) {
    return fmt::v8::format_to(ctx.out(), "{}", s.toStdString());
  }
};

template <>
struct fmt::formatter<QUuid> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

  template <typename Context>
  auto format(const QUuid &s, Context &ctx) {
    return fmt::v8::format_to(ctx.out(), "{}", s.toString().toStdString());
  }
};

template <>
struct fmt::formatter<QVector3D> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

  template <typename Context>
  auto format(const QVector3D &v, Context &ctx) {
    return fmt::v8::format_to(ctx.out(), "({}/{}/{})", v.x(), v.y(), v.z());
  }
};

template <>
struct fmt::formatter<QJsonObject> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

  template <typename Context>
  auto format(const QJsonObject &v, Context &ctx) {
    return fmt::v8::format_to(ctx.out(), "{}",
                              QJsonDocument(v).toJson(QJsonDocument::Indented).toStdString());
  }
};

template <>
struct fmt::formatter<QRectF> {
  constexpr auto parse(format_parse_context &ctx) { return ctx.end(); }

  template <typename Context>
  auto format(const QRectF &rect, Context &ctx) {
    return fmt::v8::format_to(ctx.out(), "[{}/{} {}/{}]", rect.left(), rect.top(), rect.width(),
                              rect.height());
  }
};
