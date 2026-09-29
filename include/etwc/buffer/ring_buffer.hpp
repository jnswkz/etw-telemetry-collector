#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace etwc {

// Step 3 — Ring Buffer trong RAM (Producer/Consumer).
// Bounded, thread-safe. Producer = luồng ETW; Consumer = luồng dựng đồ thị.
// Mục tiêu ETW Drop Rate < 0.1%: khi đầy, chính sách mặc định là chặn Producer
// (blocking) — có thể đổi sang overwrite tùy nhu cầu.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity)
        : capacity_(capacity), buffer_(capacity) {}

    // Producer: chặn tới khi có chỗ. Trả về false nếu buffer đã bị close().
    bool push(T value) {
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, [&] { return count_ < capacity_ || closed_; });
        if (closed_) return false;
        buffer_[tail_] = std::move(value);
        tail_ = (tail_ + 1) % capacity_;
        ++count_;
        not_empty_.notify_one();
        return true;
    }

    // Consumer: chặn tới khi có phần tử. Trả về nullopt khi đã close() và rỗng.
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, [&] { return count_ > 0 || closed_; });
        if (count_ == 0) return std::nullopt;
        T value = std::move(buffer_[head_]);
        head_ = (head_ + 1) % capacity_;
        --count_;
        not_full_.notify_one();
        return value;
    }

    void close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    std::size_t size() const {
        std::lock_guard lock(mutex_);
        return count_;
    }

    // Số sự kiện bị bỏ (nếu dùng chính sách overwrite) — phục vụ đo drop rate.
    std::uint64_t dropped() const { return dropped_.load(); }

private:
    const std::size_t capacity_;
    std::vector<T> buffer_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
    bool closed_ = false;
    std::atomic<std::uint64_t> dropped_{0};

    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

}  // namespace etwc
