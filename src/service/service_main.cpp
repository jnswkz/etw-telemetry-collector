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

// Tìm file cấu hình: cạnh exe trước, rồi thư mục hiện hành; rỗng -> dùng mặc định.
std::filesystem::path resolve_config_path() {
    wchar_t buf[MAX_PATH];
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0) {
        const std::filesystem::path p =
            std::filesystem::path(buf).parent_path() / "config" / "collector.json";
        if (std::filesystem::exists(p))
            return p;
    }
    const std::filesystem::path cwd = std::filesystem::path("config") / "collector.json";
    if (std::filesystem::exists(cwd))
        return cwd;
    return {};
}

Config load_config() {
    const std::filesystem::path p = resolve_config_path();
    return p.empty() ? Config::defaults() : Config::load(p);
}

// Bật một đặc quyền cho token tiến trình hiện tại.
bool enable_privilege(LPCWSTR name) {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return false;
    bool ok = false;
    LUID luid{};
    if (LookupPrivilegeValueW(nullptr, name, &luid)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr) != 0 &&
             GetLastError() == ERROR_SUCCESS;
    }
    CloseHandle(tok);
    return ok;
}

// SeDebugPrivilege: cần để đọc PEB/token của tiến trình thuộc user khác.
void enable_debug_privilege() {
    if (enable_privilege(SE_DEBUG_NAME))
        ETWC_LOG_INFO("Đã bật SeDebugPrivilege");
    else
        ETWC_LOG_WARN("Không bật được SeDebugPrivilege (cần admin) — đọc PEB/token có thể hạn chế");
}

// Ghi Windows Event Log (best-effort; hiển thị chuỗi chèn trong Event Viewer).
void eventlog(WORD type, const std::wstring& msg) {
    HANDLE h = RegisterEventSourceW(nullptr, kServiceName);
    if (h == nullptr)
        return;
    LPCWSTR strings[1] = {msg.c_str()};
    ReportEventW(h, type, 0, 0, nullptr, 1, 0, strings, nullptr);
    DeregisterEventSource(h);
}

// Ghi lại crash cấp tiến trình (SEH) — đặt cho CẢ console lẫn service.
void install_crash_handler() {
    ::SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* ep) -> LONG {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "FATAL unhandled exception code=0x%08lX addr=%p",
                      ep->ExceptionRecord->ExceptionCode,
                      static_cast<void*>(ep->ExceptionRecord->ExceptionAddress));
        ETWC_LOG_ERROR(buf);
        log_shutdown();
        return EXCEPTION_EXECUTE_HANDLER;
    });
}

// Khởi tạo logging (vào %ProgramData%, không phải cwd/System32), crash handler,
// rồi nạp + chuẩn hóa đường dẫn config. Trả về Config đã sẵn sàng.
Config boot(bool console_echo) {
    Config def = Config::defaults();
    def.resolve_paths();
    log_init(def.log_path.string());
    if (console_echo)
        log_set_console_echo(true);
    install_crash_handler();

    Config cfg = load_config();
    cfg.resolve_paths();
    if (cfg.log_path != def.log_path)
        log_init(cfg.log_path.string());
    return cfg;
}

void WINAPI service_main(DWORD, LPWSTR*) {
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status_handle = RegisterServiceCtrlHandlerW(kServiceName, service_ctrl_handler);
    if (!g_status_handle)
        return;

    set_state(SERVICE_START_PENDING, 3000);

    Config cfg = boot(/*console_echo=*/false);
    enable_debug_privilege();
    eventlog(EVENTLOG_INFORMATION_TYPE, L"EtwTelemetryCollector service starting");

    Collector collector(cfg);
    collector.start();
    set_state(SERVICE_RUNNING);

    while (!g_stop_requested) {
        Sleep(200);
    }

    collector.stop();
    eventlog(EVENTLOG_INFORMATION_TYPE, L"EtwTelemetryCollector service stopped");
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
    Config cfg = boot(/*console_echo=*/true);
    enable_debug_privilege();

    std::puts("=== ETW Telemetry Collector (console) ===");
    std::printf("Log: %s\n", cfg.log_path.string().c_str());
    std::printf("DB:  %s\n", cfg.sqlite_path.string().c_str());
    std::puts("Nhan Ctrl+C de dung. Can quyen Administrator de mo ETW session.\n");
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
    Config cfg = boot(/*console_echo=*/true);
    std::puts("=== SELFTEST: bơm event mẫu vào SQLite ===");

    SqliteStore store(cfg);
    store.open();
    if (!store.is_open()) {
        std::printf("Lỗi: không mở được DB (xem %s).\n", cfg.log_path.string().c_str());
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

    // Xuất đồ thị cạnh DB (dưới cùng thư mục dữ liệu).
    std::error_code ec;
    const std::filesystem::path data_dir = cfg.sqlite_path.parent_path();
    std::filesystem::create_directories(data_dir, ec);
    const std::filesystem::path dot_path = data_dir / "graph.dot";
    const std::filesystem::path json_path = data_dir / "graph.json";
    if (std::ofstream dot(dot_path); dot)
        dot << graph.to_dot();
    if (std::ofstream js(json_path); js)
        js << graph.to_json();

    std::printf("Đã ghi. Tổng bản ghi trong DB: %lld\n", static_cast<long long>(n));
    std::printf("File DB:    %s\n", cfg.sqlite_path.string().c_str());
    std::printf("Đồ thị:     %s\n", dot_path.string().c_str());
    std::printf("Graph: %zu nút, %zu cạnh\n", graph.vertex_count(), graph.edge_count());
    log_shutdown();
    return 0;
}

}  // namespace etwc::service
