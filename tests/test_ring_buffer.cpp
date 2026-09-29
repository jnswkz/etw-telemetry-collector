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
