#include <windows.h>

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "etwc/common/config.hpp"
#include "etwc/common/logging.hpp"
#include "etwc/common/uuid.hpp"
#include "etwc/graph/behavior_graph.hpp"
#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/service/collector.hpp"
#include "etwc/service/windows_service.hpp"
#include "etwc/storage/sqlite_store.hpp"

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

int run_selftest() {
    Config cfg = Config::defaults();
    log_init(cfg.log_path.string());
    log_set_console_echo(true);
    std::puts("=== SELFTEST: bơm event mẫu vào SQLite ===");

    SqliteStore store(cfg);
    store.open();
    if (!store.is_open()) {
        std::puts("Lỗi: không mở được DB (xem logs/collector.log).");
        return 1;
    }

    auto mk = [](EventKind kind, Pid pid, Pid ppid, const char* pname, const char* parent,
                 const char* target) {
        NormalizedEvent ev;
        ev.uuid = generate_uuid_v4();
        ev.kind = kind;
        ev.timestamp = static_cast<Timestamp>(::GetTickCount64());
        ev.pid = pid;
        ev.ppid = ppid;
        ev.process_name = pname;
        ev.parent_name = parent;
        ev.target = target;
        return ev;
    };

    NormalizedEvent net =
        mk(EventKind::NetConnect, 4321, 1234, "C:\\Windows\\notepad.exe", "", "93.184.216.34:443");
    net.remote_addr = "93.184.216.34";
    net.remote_port = 443;

    std::vector<NormalizedEvent> events = {
        mk(EventKind::ProcessCreate, 4321, 1234, "C:\\Windows\\notepad.exe",
           "C:\\Windows\\explorer.exe", ""),
        mk(EventKind::FileWrite, 4321, 1234, "C:\\Windows\\notepad.exe", "",
           "C:\\Users\\me\\a.txt"),
        mk(EventKind::RegSetValue, 4321, 1234, "C:\\Windows\\notepad.exe", "",
           "\\REGISTRY\\MACHINE\\SOFTWARE\\X\\Run"),
        net,
        mk(EventKind::ProcessTerminate, 4321, 1234, "C:\\Windows\\notepad.exe", "", ""),
    };

    // Ghi SQLite đồng thời dựng đồ thị nhân quả.
    BehaviorGraph graph;
    for (const NormalizedEvent& e : events) {
        store.append(e);
        graph.ingest(e);
    }
    store.flush();
    const std::int64_t n = store.count_events();
    store.close();

    // Xuất đồ thị để trực quan hóa.
    std::error_code ec;
    std::filesystem::create_directories("data", ec);
    if (std::ofstream dot("data/graph.dot"); dot)
        dot << graph.to_dot();
    if (std::ofstream js("data/graph.json"); js)
        js << graph.to_json();

    std::printf("Đã ghi. Tổng bản ghi trong DB: %lld\n", static_cast<long long>(n));
    std::printf("File DB:    %s\n", cfg.sqlite_path.string().c_str());
    std::printf("Đồ thị:     data/graph.dot (Graphviz), data/graph.json\n");
    std::printf("Graph: %zu nút, %zu cạnh\n", graph.vertex_count(), graph.edge_count());
    log_shutdown();
    return 0;
}

}  // namespace etwc::service
