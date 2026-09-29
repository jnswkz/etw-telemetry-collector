#pragma once

#include <cstdint>

#include "etwc/sensor/raw_event.hpp"

// Định danh và cờ keyword của các kernel provider được đăng ký (step 1).
// Chỉ 4 nhóm cốt lõi để loại dữ liệu dư thừa: Process / File / Registry / Network.
// Keyword bitmask đã được xác minh bằng `logman query providers "<name>"`.
namespace etwc::providers {

// Tên provider (để log/tham chiếu).
inline constexpr wchar_t kKernelProcessName[] = L"Microsoft-Windows-Kernel-Process";
inline constexpr wchar_t kKernelFileName[] = L"Microsoft-Windows-Kernel-File";
inline constexpr wchar_t kKernelRegistryName[] = L"Microsoft-Windows-Kernel-Registry";
inline constexpr wchar_t kKernelNetworkName[] = L"Microsoft-Windows-Kernel-Network";

// GUID provider (khởi tạo krabs::provider<> TRỰC TIẾP bằng GUID — tránh
// constructor theo tên vốn duyệt toàn bộ catalog qua COM/PLA rất chậm).
// GUID xác minh bằng `logman query providers "<name>"`.
inline constexpr wchar_t kKernelProcess[] = L"{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}";
inline constexpr wchar_t kKernelFile[] = L"{EDD08927-9CC4-4E65-B970-C2560FB5C289}";
inline constexpr wchar_t kKernelRegistry[] = L"{70EB4F03-C1DE-4F73-A051-33D13D5413BD}";
inline constexpr wchar_t kKernelNetwork[] = L"{7DD42A49-5329-4832-8DFD-43D979153A88}";

// --- Keyword masks (any-of) ---
// Kernel-Process: WINEVENT_KEYWORD_PROCESS (process start/stop).
inline constexpr std::uint64_t kProcessKeywords = 0x10;

// Kernel-File: CREATE | READ | WRITE | DELETE_PATH | RENAME_SETLINK_PATH | FILENAME.
inline constexpr std::uint64_t kFileKeywords = 0x80 | 0x100 | 0x200 | 0x400 | 0x800 | 0x10;

// Kernel-Registry: SetValueKey | DeleteValueKey | CreateKey | DeleteKey.
inline constexpr std::uint64_t kRegistryKeywords = 0x100 | 0x200 | 0x1000 | 0x4000;

// Kernel-Network: IPV4 | IPV6.
inline constexpr std::uint64_t kNetworkKeywords = 0x10 | 0x20;

// --- Event IDs cần quan tâm (dùng ở Normalizer / T2.1) ---
namespace process_event {
inline constexpr std::uint16_t kStart = 1;  // ProcessStart
inline constexpr std::uint16_t kStop = 2;   // ProcessStop
}  // namespace process_event

namespace file_event {
inline constexpr std::uint16_t kCreate = 12;      // Create
inline constexpr std::uint16_t kRead = 15;        // Read
inline constexpr std::uint16_t kWrite = 16;       // Write
inline constexpr std::uint16_t kDeletePath = 26;  // DeletePath
inline constexpr std::uint16_t kRenamePath = 27;  // RenamePath
}  // namespace file_event

namespace registry_event {
inline constexpr std::uint16_t kCreateKey = 1;    // CreateKey
inline constexpr std::uint16_t kSetValueKey = 5;  // SetValueKey
inline constexpr std::uint16_t kDeleteKey = 3;    // DeleteKey
inline constexpr std::uint16_t kDeleteValueKey = 6;
}  // namespace registry_event

namespace network_event {
// TCP connect (outbound): IPv4 = 12, IPv6 = 28 (KERNEL_NETWORK_TASK_TCPIP).
inline constexpr std::uint16_t kTcpConnectV4 = 12;
inline constexpr std::uint16_t kTcpConnectV6 = 28;
}  // namespace network_event

}  // namespace etwc::providers
