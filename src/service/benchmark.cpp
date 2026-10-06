#include <windows.h>
// <psapi.h> sau <windows.h>.
#include <psapi.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "etwc/buffer/ring_buffer.hpp"
#include "etwc/common/config.hpp"
#include "etwc/graph/behavior_graph.hpp"
#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/normalizer/normalizer.hpp"
#include "etwc/sensor/providers.hpp"
#include "etwc/sensor/raw_event.hpp"
#include "etwc/service/windows_service.hpp"
#include "etwc/storage/sqlite_store.hpp"

#pragma comment(lib, "psapi.lib")

namespace etwc::service {
namespace {

using clk = std::chrono::steady_clock;

double secs(clk::time_point a, clk::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}

double rss_mb() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (!::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc)))
        return 0.0;
    return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
}

void bench_normalizer() {
    constexpr int kN = 200000;
    std::vector<RawEvent> raws;
    raws.reserve(kN);
    for (int i = 0; i < kN; ++i) {
        RawEvent r;
        r.provider = ProviderId::KernelFile;
        r.event_id = providers::file_event::kWrite;
        r.pid = static_cast<Pid>(1000 + (i % 500));
        r.properties.emplace("FileObject", "0x" + std::to_string(i % 500));
        r.properties.emplace("FileName", "C:\\x\\f" + std::to_string(i % 1000) + ".txt");
        raws.push_back(std::move(r));
    }
    Normalizer n;
    std::size_t ok = 0;
    const auto t0 = clk::now();
    for (const RawEvent& r : raws)
        if (n.normalize(r))
            ++ok;
    const auto t1 = clk::now();
    std::printf("Normalizer:   %9.0f ev/s  (%d events, %zu normalized, %.2fs)\n", kN / secs(t0, t1),
                kN, ok, secs(t0, t1));
}

void bench_graph() {
    constexpr int kM = 200000;
    BehaviorGraph g;
    const double rss0 = rss_mb();
    const auto t0 = clk::now();
    for (int i = 0; i < kM; ++i) {
        NormalizedEvent e;
        e.kind = EventKind::FileWrite;
        e.pid = static_cast<Pid>(1000 + (i % 2000));
        e.process_name = "app.exe";
        e.target = "C:\\x\\f" + std::to_string(i % 20000) + ".txt";
        e.timestamp = static_cast<Timestamp>(i);
        g.ingest(e);
    }
    const auto t1 = clk::now();
    const double rss1 = rss_mb();
    std::printf("Graph ingest: %9.0f ev/s  (%zu nodes, %zu edges, RAM +%.1f -> %.1f MB)\n",
                kM / secs(t0, t1), g.vertex_count(), g.edge_count(), rss1 - rss0, rss1);
}

void bench_sqlite() {
    constexpr int kK = 100000;
    const auto dir = std::filesystem::temp_directory_path() /
                     ("etwc_bench_" + std::to_string(::GetTickCount64()));
    std::filesystem::create_directories(dir);
    Config cfg = Config::defaults();
    cfg.enable_sqlite = true;
    cfg.sqlite_path = dir / "b.sqlite";

    SqliteStore store(cfg);
    store.open();
    const auto t0 = clk::now();
    for (int i = 0; i < kK; ++i) {
        NormalizedEvent e;
        e.uuid = "u" + std::to_string(i);
        e.kind = EventKind::FileWrite;
        e.pid = static_cast<Pid>(1000 + i);
        e.process_name = "app.exe";
        e.target = "C:\\x.txt";
        store.append(e);
    }
    store.flush();
    const auto t1 = clk::now();
    const std::int64_t rows = store.count_events();
    store.close();
    std::printf("SQLite write: %9.0f ev/s  (%lld rows, %.2fs)\n", kK / secs(t0, t1),
                static_cast<long long>(rows), secs(t0, t1));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void bench_ring_buffer() {
    constexpr int kP = 1000000;
    RingBuffer<int> rb(65536);
    std::atomic<long long> popped{0};
    std::thread consumer([&] {
        while (auto v = rb.pop()) {
            (void)*v;
            popped.fetch_add(1, std::memory_order_relaxed);
        }
    });
    const auto t0 = clk::now();
    for (int i = 0; i < kP; ++i) rb.push(i);
    rb.close();
    consumer.join();
    const auto t1 = clk::now();
    std::printf("RingBuffer:   %9.0f ev/s  (%d pushed, %lld popped, dropped=%llu)\n",
                kP / secs(t0, t1), kP, popped.load(),
                static_cast<unsigned long long>(rb.dropped()));
}

}  // namespace

int run_benchmark() {
    std::puts("=== BENCHMARK (dữ liệu tổng hợp, không cần admin) ===");
    std::printf("RAM khởi điểm: %.1f MB\n\n", rss_mb());
    bench_normalizer();
    bench_graph();
    bench_sqlite();
    bench_ring_buffer();
    std::puts("");
    std::puts("Mục tiêu: SQLite >= 50k ev/s; drop rate < 0.1% (blocking -> 0);");
    std::puts("RAM đồ thị giữ 20-40 MB sau cắt tỉa (Pruner, chạy dài hạn).");
    return 0;
}

}  // namespace etwc::service
