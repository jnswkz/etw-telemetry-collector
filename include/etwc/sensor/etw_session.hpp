#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

#include "etwc/common/config.hpp"
#include "etwc/sensor/raw_event.hpp"

namespace etwc {

// Step 1 — Lớp cảm biến kernel (tương đương LSM hook của CamFlow).
// Bọc KRABSETW: khởi tạo kernel trace session, đăng ký 4 nhóm provider,
// và đẩy RawEvent ra ngoài qua callback (Producer của Ring Buffer).
class EtwSession {
public:
    using RawEventSink = std::function<void(RawEvent&&)>;

    explicit EtwSession(const Config& cfg);
    ~EtwSession();

    EtwSession(const EtwSession&) = delete;
    EtwSession& operator=(const EtwSession&) = delete;

    // Đăng ký các provider (Process/File/Registry/Network) theo config.
    void configure_providers();

    // Bắt đầu vòng lặp trace trên thread riêng. Cần quyền Administrator.
    void start(RawEventSink sink);
    void stop();

    // Số sự kiện đã nhận từ ETW (phục vụ kiểm chứng DoD / đo throughput).
    std::uint64_t events_received() const { return events_received_.load(); }

private:
    // Callback nội bộ: dựng RawEvent từ EVENT_RECORD rồi đẩy vào sink_.
    void on_raw_record(ProviderId provider, const void* record, const void* trace_context);

    const Config& cfg_;
    RawEventSink sink_;
    std::thread trace_thread_;
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> events_received_{0};

    struct Impl;  // che giấu krabs::user_trace + providers
    std::unique_ptr<Impl> impl_;
};

}  // namespace etwc
