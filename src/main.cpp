#include <windows.h>

#include <cstring>
#include <cwchar>

#include "etwc/service/windows_service.hpp"

// Entry point. Định tuyến theo tham số dòng lệnh:
//   (không tham số)   -> chạy như Windows Service (do SCM gọi)
//   --console         -> chạy foreground để debug
//   --selftest        -> bơm event mẫu vào SQLite + xuất đồ thị
//   --bench           -> đo hiệu năng
//   --install         -> đăng ký service
//   --uninstall       -> gỡ service
int wmain(int argc, wchar_t** argv) {
    using namespace etwc::service;

    // Console dùng UTF-8 để log/tiếng Việt hiển thị đúng (chuỗi nguồn là UTF-8).
    ::SetConsoleOutputCP(CP_UTF8);

    if (argc >= 2) {
        if (std::wcscmp(argv[1], L"--console") == 0)
            return run_as_console();
        if (std::wcscmp(argv[1], L"--selftest") == 0)
            return run_selftest();
        if (std::wcscmp(argv[1], L"--bench") == 0)
            return run_benchmark();
        if (std::wcscmp(argv[1], L"--install") == 0)
            return install();
        if (std::wcscmp(argv[1], L"--uninstall") == 0)
            return uninstall();
    }
    return run_as_service();
}
