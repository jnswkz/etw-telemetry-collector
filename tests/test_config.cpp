#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "etwc/common/config.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

namespace {
std::filesystem::path write_temp(const std::string& content) {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto p = std::filesystem::temp_directory_path() /
                   ("etwc_cfg_" + std::to_string(static_cast<unsigned long long>(ns)) + ".json");
    std::ofstream(p) << content;
    return p;
}
}  // namespace

TEST_CASE("Config::load overrides defaults and keeps missing keys", "[config]") {
    const auto p = write_temp(R"({
        "ring_capacity": 1000,
        "storage": { "enable_sqlite": false, "sqlite_path": "x/custom.sqlite" },
        "sensor": { "trace_file": false },
        "graph": { "prune_interval_ms": 2000, "soft_vertex_limit": 50000 }
    })");

    etwc::Config cfg = etwc::Config::load(p);
    REQUIRE(cfg.ring_capacity == 1024);  // 1000 làm tròn lên lũy thừa 2
    REQUIRE(cfg.enable_sqlite == false);
    REQUIRE(cfg.sqlite_path.string() == "x/custom.sqlite");
    REQUIRE(cfg.trace_file == false);
    REQUIRE(cfg.trace_process == true);  // khóa thiếu -> giữ mặc định
    REQUIRE(cfg.prune_interval_ms == 2000);
    REQUIRE(cfg.soft_vertex_limit == 50000);

    std::error_code ec;
    std::filesystem::remove(p, ec);
}

TEST_CASE("Config::load returns defaults for missing file", "[config]") {
    etwc::Config cfg = etwc::Config::load("no_such_dir/does_not_exist.json");
    etwc::Config def = etwc::Config::defaults();
    REQUIRE(cfg.ring_capacity == def.ring_capacity);
    REQUIRE(cfg.enable_sqlite == def.enable_sqlite);
}

TEST_CASE("Config::load clamps invalid values", "[config]") {
    const auto p =
        write_temp(R"({ "graph": { "prune_interval_ms": 1, "soft_vertex_limit": 10 } })");
    etwc::Config cfg = etwc::Config::load(p);
    REQUIRE(cfg.prune_interval_ms >= 100);
    REQUIRE(cfg.soft_vertex_limit >= 1000);
    std::error_code ec;
    std::filesystem::remove(p, ec);
}
#endif
