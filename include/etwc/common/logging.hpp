#pragma once

#include <string_view>

namespace etwc {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

// Minimal thread-safe logger. Replace/extend with spdlog if desired.
void log_init(std::string_view file_path, LogLevel min_level = LogLevel::Info);
void log_write(LogLevel level, std::string_view msg);

#define ETWC_LOG_INFO(msg) ::etwc::log_write(::etwc::LogLevel::Info, (msg))
#define ETWC_LOG_WARN(msg) ::etwc::log_write(::etwc::LogLevel::Warn, (msg))
#define ETWC_LOG_ERROR(msg) ::etwc::log_write(::etwc::LogLevel::Error, (msg))

}  // namespace etwc
