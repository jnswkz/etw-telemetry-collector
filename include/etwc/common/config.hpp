#pragma once

#include <cstddef>
#include <filesystem>
#include <string>

namespace etwc {

// Runtime configuration, loaded from config/collector.json (see config/).
struct Config {
    // --- Ring buffer (step 3) ---
    std::size_t ring_capacity = 65536;  // must be a power of two

    // --- Storage (step 3b) ---
    std::filesystem::path sqlite_path = "data/telemetry.sqlite";
    bool enable_sqlite = true;

    // --- Sensor (step 1) ---
    bool trace_process = true;
    bool trace_file = true;
    bool trace_registry = true;
    bool trace_network = true;

    // --- Graph pruning (step 5) ---
    std::size_t prune_interval_ms = 5000;
    std::size_t soft_vertex_limit = 200000;

    // --- Logging ---
    std::filesystem::path log_path = "logs/collector.log";

    static Config load(const std::filesystem::path& file);
    static Config defaults();
};

}  // namespace etwc
