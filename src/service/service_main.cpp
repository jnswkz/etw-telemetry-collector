#include <windows.h>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string>

#include "etwc/common/config.hpp"
#include "etwc/common/logging.hpp"
#include "etwc/service/collector.hpp"
#include "etwc/service/windows_service.hpp"

// Vòng đời Windows Service + chế độ console để debug.
namespace etwc::service {
namespace {

SERVICE_STATUS_HANDLE g_status_handle = nullptr;
SERVICE_STATUS g_status{};
std::atomic<bool> g_stop_requested{false};

void set_state(DWORD state, DWORD wait_hint = 0) {
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted = (state == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;
    g_status.dwWaitHint = wait_hint;
    if (g_status_handle)
        SetServiceStatus(g_status_handle, &g_status);
}

void WINAPI service_ctrl_handler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        set_state(SERVICE_STOP_PENDING, 3000);
        g_stop_requested = true;
    }
}

void WINAPI service_main(DWORD, LPWSTR*) {
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status_handle = RegisterServiceCtrlHandlerW(kServiceName, service_ctrl_handler);
    if (!g_status_handle)
        return;

    set_state(SERVICE_START_PENDING, 3000);

    Config cfg = Config::defaults();
    log_init(cfg.log_path.string());
    Collector collector(cfg);
    collector.start();
    set_state(SERVICE_RUNNING);

    while (!g_stop_requested) {
        Sleep(200);
    }

    collector.stop();
    log_shutdown();
    set_state(SERVICE_STOPPED);
}

}  // namespace

int run_as_service() {
    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<LPWSTR>(kServiceName), service_main},
        {nullptr, nullptr},
    };
    return StartServiceCtrlDispatcherW(table) ? 0 : 1;
}

int run_as_console() {
    Config cfg = Config::defaults();
    log_init(cfg.log_path.string());
    log_set_console_echo(true);  // in log ra màn hình cho chế độ console

    // Bắt crash cấp tiến trình (SEH) để ghi lại nguyên nhân trước khi chết.
    ::SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "FATAL unhandled exception code=0x%08lX addr=%p",
                      ep->ExceptionRecord->ExceptionCode,
                      static_cast<void*>(ep->ExceptionRecord->ExceptionAddress));
        ETWC_LOG_ERROR(buf);
        log_shutdown();
        return EXCEPTION_EXECUTE_HANDLER;
    });

    std::puts("=== ETW Telemetry Collector (console) ===");
    std::puts("Log: logs/collector.log | Nhan Ctrl+C de dung.");
    std::puts("Luu y: can chay bang quyen Administrator de mo ETW session.\n");
    std::fflush(stdout);

    Collector collector(cfg);
    collector.start();

    ETWC_LOG_INFO("Running in console mode. Press Ctrl+C to stop.");
    static std::atomic<bool> stop{false};
    std::signal(SIGINT, [](int) { stop = true; });

    // In số liệu mỗi giây để kiểm chứng luồng sự kiện (DoD Phase 1).
    std::uint64_t last = 0;
    int ticks = 0;
    while (!stop) {
        Sleep(200);
        if (++ticks >= 5) {  // ~1s
            ticks = 0;
            const std::uint64_t recv = collector.events_received();
            const std::uint64_t ing = collector.events_ingested();
            ETWC_LOG_INFO("ETW events/s=" + std::to_string(recv - last) + " total_received=" +
                          std::to_string(recv) + " ingested=" + std::to_string(ing));
            last = recv;
        }
    }

    collector.stop();
    log_shutdown();
    return 0;
}

}  // namespace etwc::service
