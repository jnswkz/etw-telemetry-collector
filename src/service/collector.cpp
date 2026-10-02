#include "etwc/service/collector.hpp"

#include <chrono>

#include "etwc/common/logging.hpp"

namespace etwc {

Collector::Collector(Config cfg)
    : cfg_(std::move(cfg)),
      ring_(cfg_.ring_capacity),
      pruner_(graph_),
      store_(cfg_),
      sensor_(cfg_) {}

Collector::~Collector() {
    stop();
}

void Collector::start() {
    if (running_.exchange(true))
        return;
    store_.open();

    // Consumer: rút NormalizedEvent khỏi ring -> graph + storage + prune.
    consumer_thread_ = std::thread([this] { consumer_loop(); });

    // Producer: ETW sensor -> Normalizer -> ring buffer.
    sensor_.start([this](RawEvent&& raw) {
        if (auto ev = normalizer_.normalize(raw)) {
            ring_.push(std::move(*ev));
        }
    });

    ETWC_LOG_INFO("Collector started");
}

void Collector::consumer_loop() {
    using clock = std::chrono::steady_clock;
    auto last_flush = clock::now();
    std::size_t since_prune = 0;

    auto process = [&](NormalizedEvent& ev) {
        graph_.ingest(ev);
        store_.append(ev);
        events_ingested_.fetch_add(1, std::memory_order_relaxed);
        if (++since_prune >= 1000) {
            pruner_.run_cycle();
            since_prune = 0;
        }
    };

    while (running_) {
        // Thức dậy định kỳ (200ms) để flush/prune ngay cả khi lưu lượng thấp.
        if (auto ev = ring_.pop_for(std::chrono::milliseconds(200)))
            process(*ev);
        if (clock::now() - last_flush >= std::chrono::seconds(1)) {
            store_.flush();
            last_flush = clock::now();
        }
    }

    // Rút nốt phần còn lại sau khi dừng rồi flush lần cuối.
    while (auto ev = ring_.pop_for(std::chrono::milliseconds(0))) process(*ev);
    store_.flush();
}

void Collector::stop() {
    if (!running_.exchange(false))
        return;
    sensor_.stop();
    ring_.close();
    if (consumer_thread_.joinable())
        consumer_thread_.join();
    store_.close();
    ETWC_LOG_INFO("Collector stopped");
}

}  // namespace etwc
