#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

#include "etwc/common/config.hpp"
#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/storage/sqlite_store.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

namespace {

std::filesystem::path unique_db_dir() {
    const auto ns = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("etwc_test_" + std::to_string(static_cast<unsigned long long>(ns)));
}

etwc::NormalizedEvent make_event(int i) {
    etwc::NormalizedEvent ev;
    ev.uuid = "uuid-" + std::to_string(i);
    ev.kind = etwc::EventKind::FileWrite;
    ev.timestamp = 1000 + i;
    ev.pid = 100 + i;
    ev.ppid = 4;
    ev.process_name = "app.exe";
    ev.parent_name = "explorer.exe";
    ev.target = "C:\\data\\f.txt";
    ev.token_elevated = true;
    return ev;
}

}  // namespace

TEST_CASE("SqliteStore persists events and dedups by uuid", "[storage]") {
    const auto dir = unique_db_dir();
    std::filesystem::create_directories(dir);
    etwc::Config cfg = etwc::Config::defaults();
    cfg.enable_sqlite = true;
    cfg.sqlite_path = dir / "t.sqlite";

    {
        etwc::SqliteStore store(cfg);
        store.open();
        REQUIRE(store.is_open());
        for (int i = 0; i < 10; ++i) store.append(make_event(i));
        REQUIRE(store.count_events() == 10);

        // uuid trùng -> INSERT OR IGNORE giữ nguyên số bản ghi.
        store.append(make_event(0));
        REQUIRE(store.count_events() == 10);
        store.close();
    }

    // Mở lại: dữ liệu bền vững trên đĩa.
    {
        etwc::SqliteStore store(cfg);
        store.open();
        REQUIRE(store.count_events() == 10);
        store.close();
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("SqliteStore batch flush at threshold", "[storage]") {
    const auto dir = unique_db_dir();
    std::filesystem::create_directories(dir);
    etwc::Config cfg = etwc::Config::defaults();
    cfg.enable_sqlite = true;
    cfg.sqlite_path = dir / "t.sqlite";

    etwc::SqliteStore store(cfg);
    store.open();
    for (int i = 0; i < 1500; ++i)  // > ngưỡng flush (512) nhiều lần
        store.append(make_event(i));
    REQUIRE(store.count_events() == 1500);
    store.close();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("SqliteStore disabled is a no-op", "[storage]") {
    etwc::Config cfg = etwc::Config::defaults();
    cfg.enable_sqlite = false;

    etwc::SqliteStore store(cfg);
    store.open();
    REQUIRE_FALSE(store.is_open());
    store.append(make_event(1));  // không crash
    store.flush();
    store.close();
}
#endif
