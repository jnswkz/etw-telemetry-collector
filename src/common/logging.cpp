#include "etwc/common/logging.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

namespace etwc {
namespace {

std::mutex g_mutex;
LogConfig g_cfg;
FILE* g_file = nullptr;
std::size_t g_written_bytes = 0;

const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Trace:
            return "TRACE";
        case LogLevel::Debug:
            return "DEBUG";
        case LogLevel::Info:
            return "INFO";
        case LogLevel::Warn:
            return "WARN";
        case LogLevel::Error:
            return "ERROR";
    }
    return "?";
}

// yyyy-MM-dd HH:mm:ss.mmm (giờ địa phương).
std::string now_timestamp() {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto t = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm tm{};
    localtime_s(&tm, &t);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d", tm.tm_year + 1900,
                  tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

std::string this_thread_id() {
    std::ostringstream oss;
    oss << std::this_thread::get_id();
    return oss.str();
}

void open_file_locked() {
    if (g_cfg.file_path.empty())
        return;
    const std::filesystem::path path{g_cfg.file_path};
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
    }
    g_file = nullptr;
    fopen_s(&g_file, path.string().c_str(), "a");
    if (g_file) {
        std::fseek(g_file, 0, SEEK_END);
        g_written_bytes = static_cast<std::size_t>(std::ftell(g_file));
    }
}

// Xoay vòng: collector.log -> .1, .1 -> .2, ... xóa file cũ nhất.
void rotate_locked() {
    if (!g_file || g_cfg.file_path.empty())
        return;
    std::fclose(g_file);
    g_file = nullptr;

    const std::filesystem::path base{g_cfg.file_path};
    std::error_code ec;
    // Xóa file cũ nhất.
    std::filesystem::remove(base.string() + "." + std::to_string(g_cfg.max_rotated_files), ec);
    for (int i = g_cfg.max_rotated_files - 1; i >= 1; --i) {
        std::filesystem::rename(base.string() + "." + std::to_string(i),
                                base.string() + "." + std::to_string(i + 1), ec);
    }
    std::filesystem::rename(base, base.string() + ".1", ec);

    g_written_bytes = 0;
    open_file_locked();
}

}  // namespace

void log_init(const LogConfig& cfg) {
    std::lock_guard lock(g_mutex);
    g_cfg = cfg;
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    open_file_locked();
}

void log_init(std::string_view file_path, LogLevel min_level) {
    LogConfig cfg;
    cfg.file_path = std::string(file_path);
    cfg.min_level = min_level;
    log_init(cfg);
}

void log_write(LogLevel level, std::string_view msg) {
    if (level < g_cfg.min_level)
        return;

    std::string line = now_timestamp();
    line += " [";
    line += level_name(level);
    line += "] (t";
    line += this_thread_id();
    line += ") ";
    line.append(msg.data(), msg.size());
    line += '\n';

    std::lock_guard lock(g_mutex);
    if (g_file && g_written_bytes + line.size() > g_cfg.max_file_bytes) {
        rotate_locked();
    }
    FILE* out = g_file ? g_file : stderr;
    std::fwrite(line.data(), 1, line.size(), out);
    std::fflush(out);
    if (g_file)
        g_written_bytes += line.size();
}

void log_shutdown() {
    std::lock_guard lock(g_mutex);
    if (g_file) {
        std::fflush(g_file);
        std::fclose(g_file);
        g_file = nullptr;
    }
}

}  // namespace etwc
