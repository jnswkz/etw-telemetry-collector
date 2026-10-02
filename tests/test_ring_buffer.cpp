#include <atomic>
#include <thread>
#include <vector>

#include "etwc/buffer/ring_buffer.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

TEST_CASE("RingBuffer producer/consumer FIFO", "[buffer]") {
    etwc::RingBuffer<int> rb(4);
    REQUIRE(rb.push(1));
    REQUIRE(rb.push(2));
    REQUIRE(rb.pop() == 1);
    REQUIRE(rb.pop() == 2);
}

TEST_CASE("RingBuffer close unblocks consumer", "[buffer]") {
    etwc::RingBuffer<int> rb(2);
    rb.close();
    REQUIRE_FALSE(rb.pop().has_value());
}

// T3.1 — stress: nhiều producer, một consumer, buffer nhỏ -> không mất phần tử.
TEST_CASE("RingBuffer multi-producer loses nothing", "[buffer]") {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 5000;
    etwc::RingBuffer<int> rb(64);  // nhỏ để ép producer chặn/giành nhau

    std::atomic<long long> sum{0};
    std::atomic<int> popped{0};
    std::thread consumer([&] {
        while (auto v = rb.pop()) {
            sum.fetch_add(*v, std::memory_order_relaxed);
            popped.fetch_add(1, std::memory_order_relaxed);
        }
    });

    long long expected = 0;
    for (int p = 0; p < kProducers; ++p)
        for (int i = 1; i <= kPerProducer; ++i) expected += (p * kPerProducer + i);

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&rb, p] {
            for (int i = 1; i <= kPerProducer; ++i) rb.push(p * kPerProducer + i);
        });
    }
    for (auto& t : producers) t.join();
    rb.close();
    consumer.join();

    REQUIRE(popped.load() == kProducers * kPerProducer);
    REQUIRE(sum.load() == expected);
}
#else
#include <cassert>
int test_ring_buffer_fallback() {
    etwc::RingBuffer<int> rb(4);
    assert(rb.push(1));
    assert(rb.pop() == 1);
    rb.close();
    assert(!rb.pop().has_value());
    return 0;
}
#endif
