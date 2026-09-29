#include "etwc/common/logging.hpp"

#include <cstdio>
#include <mutex>
#include <string>

namespace etwc {
namespace {
std::mutex g_mutex;
LogLevel g_min_level = LogLevel::Info;
FILE* g_file = nullptr;

const char* level_name(LogLevel l) {
    switch (l) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "?";
}
}  // namespace

void log_init(std::string_view file_path, LogLevel min_level) {
    std::lock_guard lock(g_mutex);
    g_min_level = min_level;
    if (!file_path.empty()) {
        fopen_s(&g_file, std::string(file_path).c_str(), "a");
    }
}

void log_write(LogLevel level, std::string_view msg) {
    if (level < g_min_level) return;
    std::lock_guard lock(g_mutex);
    FILE* out = g_file ? g_file : stderr;
    std::fprintf(out, "[%s] %.*s\n", level_name(level),
                 static_cast<int>(msg.size()), msg.data());
    std::fflush(out);
}

}  // namespace etwc
