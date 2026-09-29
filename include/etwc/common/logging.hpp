#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace etwc {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

// Cấu hình logger.
struct LogConfig {
    std::string file_path;  // rỗng => chỉ ghi stderr
    LogLevel min_level = LogLevel::Info;
    std::size_t max_file_bytes = 10 * 1024 * 1024;  // ngưỡng xoay vòng (10 MB)
    int max_rotated_files = 5;                      // giữ collector.log.1 .. .N
};

// Khởi tạo logger. Thread-safe; gọi một lần lúc khởi động.
void log_init(const LogConfig& cfg);
void log_init(std::string_view file_path, LogLevel min_level = LogLevel::Info);

// Bật/tắt echo log ra stdout (dùng cho chế độ --console để thấy trực tiếp).
void log_set_console_echo(bool enabled);

// Ghi một dòng log (kèm timestamp + level + thread id). Thread-safe.
void log_write(LogLevel level, std::string_view msg);

// Đóng file log (flush). Gọi khi shutdown.
void log_shutdown();

#define ETWC_LOG_TRACE(msg) ::etwc::log_write(::etwc::LogLevel::Trace, (msg))
#define ETWC_LOG_DEBUG(msg) ::etwc::log_write(::etwc::LogLevel::Debug, (msg))
#define ETWC_LOG_INFO(msg) ::etwc::log_write(::etwc::LogLevel::Info, (msg))
#define ETWC_LOG_WARN(msg) ::etwc::log_write(::etwc::LogLevel::Warn, (msg))
#define ETWC_LOG_ERROR(msg) ::etwc::log_write(::etwc::LogLevel::Error, (msg))

}  // namespace etwc
