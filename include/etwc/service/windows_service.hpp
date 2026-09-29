#pragma once

#include <string>

namespace etwc {

// Vỏ Windows Service (SCM): cài đặt, gỡ, và vòng đời control handler.
namespace service {

constexpr wchar_t kServiceName[] = L"EtwTelemetryCollector";
constexpr wchar_t kDisplayName[] = L"ETW Telemetry Collector";

int install();         // đăng ký service với SCM
int uninstall();       // gỡ service
int run_as_service();  // gọi StartServiceCtrlDispatcher
int run_as_console();  // chạy foreground để debug (Ctrl+C để dừng)

}  // namespace service
}  // namespace etwc
