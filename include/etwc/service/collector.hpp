#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "etwc/buffer/ring_buffer.hpp"
#include "etwc/common/config.hpp"
#include "etwc/graph/behavior_graph.hpp"
#include "etwc/graph/pruner.hpp"
#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/normalizer/normalizer.hpp"
#include "etwc/sensor/etw_session.hpp"
#include "etwc/storage/sqlite_store.hpp"

namespace etwc {

// Bộ điều phối toàn pipeline: nối Sensor -> Normalizer -> RingBuffer
// -> (BehaviorGraph + SqliteStore) -> Pruner.
// Được dùng cả trong Windows Service lẫn chế độ --console.
class Collector {
public:
    explicit Collector(Config cfg);
    ~Collector();

    void start();  // khởi động producer (ETW) + consumer threads
    void stop();   // dừng gọn gàng, flush storage

    // Số liệu quan trắc (phục vụ chế độ console / đo throughput).
    std::uint64_t events_received() const { return sensor_.events_received(); }
    std::uint64_t events_ingested() const { return events_ingested_.load(); }

private:
    void consumer_loop();  // rút từ ring buffer, dựng graph, prune, lưu

    Config cfg_;
    RingBuffer<NormalizedEvent> ring_;
    Normalizer normalizer_;
    BehaviorGraph graph_;
    Pruner pruner_;
    SqliteStore store_;
    EtwSession sensor_;

    std::thread consumer_thread_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> events_ingested_{0};  // đã đưa vào graph/storage
};

}  // namespace etwc
