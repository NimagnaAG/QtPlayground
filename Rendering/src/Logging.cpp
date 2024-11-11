#include "Rendering/pch.h"

#include "Rendering/Logging.h"

#include <QDir>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>

namespace nimagna {

std::string LoggerInfo::logfileLocation() {
  static const std::string kLogfileLocation = generateLogfilePath();
  return kLogfileLocation;
}

spdlog::level::level_enum LoggerInfo::logLevel() {
  return mLogLevel;
}

std::string LoggerInfo::defaultLogPattern() {
#if NIMAGNA_WINDOWS
  // the '~' pattern is defined for the FunctionNameFormatter
  return "%Y-%m-%d %H:%M:%S.%e | %-6t | %^%-8l | %~ | %-5# | %v%$";
#elif NIMAGNA_MACOS
  return "%Y-%m-%d %H:%M:%S.%e | %-6t | %^%-8l | %-25s | %-5# | %-30!! | %v%$";
#endif
}

std::string LoggerInfo::generateLogfilePath() {
  QDateTime date = QDateTime::currentDateTime();
  QString formattedTime = date.toString("yyyy-MM-dd_hh-mm-ss-zzz");
  return formattedTime.toStdString() + ".txt";
}

void FunctionNameFormatter::format(const spdlog::details::log_msg &message, const std::tm &,
                                   spdlog::memory_buf_t &dest) {
  std::string output = std::string(message.source.funcname);
  // find and remove 'nimagna::'
  constexpr int kNimagnaLength = 9;
  if (auto pos = output.find("nimagna::"); pos != std::string::npos) {
    if (pos > 0) {
      output = output.substr(0, pos - 1) +
               output.substr(pos + kNimagnaLength, output.length() - (pos + kNimagnaLength));
    } else {
      output = output.substr(pos + kNimagnaLength, output.length() - (pos + kNimagnaLength));
    }
  }
  // truncate/left align to 50
  const auto kLength = output.length();
  constexpr int kWidth = 50;
  if (kLength < kWidth) {
    output.insert(kLength, kWidth - kLength, ' ');
  } else if (kLength > kWidth) {
    output = output.substr(0, kWidth);
  }
  // append to output
  dest.append(output.data(), output.data() + output.size());
}

}  // namespace nimagna
