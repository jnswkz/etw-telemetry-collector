#pragma once

#include <atomic>
#include <functional>
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

private:
    const Config& cfg_;
    RawEventSink sink_;
    std::thread trace_thread_;
    std::atomic<bool> running_{false};

    struct Impl;                 // che giấu krabs::kernel_trace
    std::unique_ptr<Impl> impl_;
};

}  // namespace etwc
