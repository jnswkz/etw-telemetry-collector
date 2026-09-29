#include "etwc/service/collector.hpp"

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
    std::size_t since_prune = 0;
    while (running_) {
        auto ev = ring_.pop();
        if (!ev)
            break;  // ring closed
        graph_.ingest(*ev);
        store_.append(*ev);
        events_ingested_.fetch_add(1, std::memory_order_relaxed);
        if (++since_prune >= 1000) {
            pruner_.run_cycle();
            since_prune = 0;
        }
    }
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
