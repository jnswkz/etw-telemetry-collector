#include "etwc/sensor/etw_session.hpp"

#include "etwc/common/logging.hpp"
#include "etwc/sensor/providers.hpp"

// TODO: tích hợp krabs::kernel_trace / krabs::user_trace trong Impl.

namespace etwc {

struct EtwSession::Impl {
    // krabs::user_trace trace;  // hoặc kernel_trace
};

EtwSession::EtwSession(const Config& cfg)
    : cfg_(cfg), impl_(std::make_unique<Impl>()) {}

EtwSession::~EtwSession() { stop(); }

void EtwSession::configure_providers() {
    // TODO: đăng ký 4 provider theo cfg_ (process/file/registry/network),
    //       gắn callback -> chuyển thành RawEvent -> sink_.
    ETWC_LOG_INFO("EtwSession::configure_providers (stub)");
}

void EtwSession::start(RawEventSink sink) {
    sink_ = std::move(sink);
    configure_providers();
    running_ = true;
    trace_thread_ = std::thread([this] {
        ETWC_LOG_INFO("ETW trace loop started (stub)");
        // TODO: impl_->trace.start();  (blocking cho tới khi stop)
    });
}

void EtwSession::stop() {
    if (!running_.exchange(false)) return;
    // TODO: impl_->trace.stop();
    if (trace_thread_.joinable()) trace_thread_.join();
}

}  // namespace etwc
