#pragma once

namespace nimagna {
class LoggerMainApplication {
 public:
  static void configureAll(int argc, char* argv[]);
  static void setPattern(const std::string& pattern);
  static void setLogLevel(spdlog::level::level_enum level);
  static void flushAll();

 private:
  static void defaultInitialization(std::shared_ptr<spdlog::logger> logger);
};

}  // namespace nimagna
