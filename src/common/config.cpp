#include "etwc/common/config.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "etwc/common/logging.hpp"

namespace etwc {
namespace {

// Làm tròn LÊN lũy thừa 2 gần nhất (Ring Buffer dùng chỉ số vòng).
std::size_t round_up_pow2(std::size_t v) {
    if (v < 2)
        return 2;
    std::size_t p = 1;
    while (p < v) p <<= 1;
    return p;
}

}  // namespace

Config Config::defaults() {
    return Config{};
}

Config Config::load(const std::filesystem::path& file) {
    Config cfg = defaults();

    std::ifstream in(file);
    if (!in) {
        ETWC_LOG_WARN("Config: không mở được '" + file.string() + "', dùng mặc định.");
        return cfg;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        ETWC_LOG_ERROR(std::string("Config: JSON lỗi (") + e.what() + "), dùng mặc định.");
        return cfg;
    }

    // Đọc có khoan dung: thiếu khóa -> giữ mặc định.
    cfg.ring_capacity = j.value("ring_capacity", cfg.ring_capacity);

    if (auto it = j.find("storage"); it != j.end() && it->is_object()) {
        cfg.enable_sqlite = it->value("enable_sqlite", cfg.enable_sqlite);
        cfg.sqlite_path = it->value("sqlite_path", cfg.sqlite_path.string());
    }
    if (auto it = j.find("sensor"); it != j.end() && it->is_object()) {
        cfg.trace_process = it->value("trace_process", cfg.trace_process);
        cfg.trace_file = it->value("trace_file", cfg.trace_file);
        cfg.trace_registry = it->value("trace_registry", cfg.trace_registry);
        cfg.trace_network = it->value("trace_network", cfg.trace_network);
    }
    if (auto it = j.find("graph"); it != j.end() && it->is_object()) {
        cfg.prune_interval_ms = it->value("prune_interval_ms", cfg.prune_interval_ms);
        cfg.soft_vertex_limit = it->value("soft_vertex_limit", cfg.soft_vertex_limit);
    }
    if (auto it = j.find("logging"); it != j.end() && it->is_object()) {
        cfg.log_path = it->value("path", cfg.log_path.string());
    }

    // Validate / chuẩn hóa.
    const std::size_t rc = round_up_pow2(cfg.ring_capacity);
    if (rc != cfg.ring_capacity) {
        ETWC_LOG_WARN("Config: ring_capacity làm tròn lên lũy thừa 2: " + std::to_string(rc));
        cfg.ring_capacity = rc;
    }
    if (cfg.prune_interval_ms < 100)
        cfg.prune_interval_ms = 100;
    if (cfg.soft_vertex_limit < 1000)
        cfg.soft_vertex_limit = 1000;

    ETWC_LOG_INFO("Config: đã nạp từ '" + file.string() + "'");
    return cfg;
}

}  // namespace etwc
