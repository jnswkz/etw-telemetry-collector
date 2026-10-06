// Integration test: Normalizer -> RingBuffer -> consumer -> Graph + SQLite.
// Mô phỏng đường đi của Collector nhưng dùng RawEvent tổng hợp (không cần ETW/admin).
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "etwc/buffer/ring_buffer.hpp"
#include "etwc/common/config.hpp"
#include "etwc/graph/behavior_graph.hpp"
#include "etwc/normalizer/normalizer.hpp"
#include "etwc/sensor/providers.hpp"
#include "etwc/sensor/raw_event.hpp"
#include "etwc/storage/sqlite_store.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

namespace {
etwc::RawEvent proc_create(etwc::Pid pid, etwc::Pid ppid, const char* img) {
    etwc::RawEvent r;
    r.provider = etwc::ProviderId::KernelProcess;
    r.event_id = etwc::providers::process_event::kStart;
    r.pid = ppid;
    r.properties.emplace("ProcessID", std::to_string(pid));
    r.properties.emplace("ParentProcessID", std::to_string(ppid));
    r.properties.emplace("ImageName", img);
    return r;
}
etwc::RawEvent file_write(etwc::Pid pid, const char* obj, const char* name) {
    etwc::RawEvent r;
    r.provider = etwc::ProviderId::KernelFile;
    r.event_id = etwc::providers::file_event::kWrite;
    r.pid = pid;
    r.properties.emplace("FileObject", obj);
    r.properties.emplace("FileName", name);
    return r;
}
}  // namespace

TEST_CASE("Pipeline: Normalizer -> RingBuffer -> Graph + SQLite", "[pipeline]") {
    const auto dir = std::filesystem::temp_directory_path() /
                     ("etwc_pipe_" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(dir);
    etwc::Config cfg = etwc::Config::defaults();
    cfg.enable_sqlite = true;
    cfg.sqlite_path = dir / "p.sqlite";

    etwc::RingBuffer<etwc::NormalizedEvent> ring(256);
    etwc::BehaviorGraph graph;
    etwc::SqliteStore store(cfg);
    store.open();
    REQUIRE(store.is_open());

    // Consumer: rút khỏi ring -> graph + store (giống Collector::consumer_loop).
    std::size_t ingested = 0;
    std::thread consumer([&] {
        while (auto ev = ring.pop()) {
            graph.ingest(*ev);
            store.append(*ev);
            ++ingested;
        }
    });

    // Producer: tạo RawEvent -> Normalizer -> ring.
    etwc::Normalizer norm;
    std::size_t produced = 0;
    auto feed = [&](const etwc::RawEvent& raw) {
        if (auto ev = norm.normalize(raw)) {
            ring.push(std::move(*ev));
            ++produced;
        }
    };

    feed(proc_create(100, 4, "C:\\Windows\\explorer.exe"));  // explorer(4)->notepad? no, pid100
    feed(proc_create(200, 100, "C:\\app.exe"));
    for (int i = 0; i < 50; ++i)
        feed(file_write(200, "0xAB", "C:\\data\\log.txt"));  // 50 lần ghi CÙNG file

    ring.close();
    consumer.join();
    store.flush();

    REQUIRE(produced == 52);              // 2 process + 50 file
    REQUIRE(ingested == produced);        // không mất sự kiện qua ring
    REQUIRE(store.count_events() == 52);  // tất cả ghi xuống DB

    // Đồ thị: parent(4) + proc100 + proc200 + file = 4 nút;
    // cạnh: 4->100, 100->200, 200->file(gộp) = 3 cạnh (dedup 50 lần ghi).
    REQUIRE(graph.vertex_count() == 4);
    REQUIRE(graph.edge_count() == 3);
    store.close();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
#endif
