#include "pch.h"

#include "LoggerMainApplication.h"

#include "Rendering/Rendering.h"
#include "Rendering/Logging.h"

namespace nimagna {

void LoggerMainApplication::configureAll(int argc, char* argv[]) {
  // configure logger for the main application project itself
  auto stdoutSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  // whenever a log file exceeds 1MB, a new file is created (up to 100 files)
  auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
      LoggerInfo::logfileLocation(), 1024 * 1024, 100);
  auto distSink = std::make_shared<spdlog::sinks::dist_sink_st>();
  distSink->add_sink(stdoutSink);
  distSink->add_sink(fileSink);
#if NIMAGNA_WINDOWS
  auto msvcSink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
  distSink->add_sink(msvcSink);
#endif
  auto logger = std::make_shared<spdlog::logger>("NimagnaLoggerMain", distSink);
  defaultInitialization(logger);

  // configure the loggers for the other projects
  LoggerRendering::configure(distSink);

  // Process logging level parameters in the form of SPDLOG_LEVEL=info
  // https://github.com/gabime/spdlog/blob/v1.x/README.md#load-log-levels-from-env-variable-or-from-argv
  spdlog::cfg::load_argv_levels(argc, argv);
  // set current level to Base module logger
  setLogLevel(spdlog::get_level());
}

void LoggerMainApplication::setPattern(const std::string& pattern) {
  spdlog::set_pattern(pattern);
  LoggerRendering::setPattern(pattern);
}

void LoggerMainApplication::setLogLevel(spdlog::level::level_enum level) {
  spdlog::set_level(level);
  LoggerRendering::setLogLevel(level);
  SPDLOG_INFO("Log level set to {}", level);
}

void flushLogger(std::shared_ptr<spdlog::logger> logger) {
  logger->flush();
}

void LoggerMainApplication::flushAll() {
  spdlog::apply_all(&flushLogger);
}

void LoggerMainApplication::defaultInitialization(std::shared_ptr<spdlog::logger> logger) {
  // by default spdlog creates a default global logger (to stdout, colored and multithreaded).
  // this overwrites the default one with the custom logger
  spdlog::set_default_logger(std::move(logger));
  setPattern(LoggerInfo::defaultLogPattern());
  setLogLevel(LoggerInfo::logLevel());
  spdlog::flush_on(spdlog::level::err);
}

}  // namespace nimagna
