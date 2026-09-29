#pragma once

#include <cstdint>

// GUID và cờ keyword của các kernel provider được đăng ký (step 1).
// Chỉ 4 nhóm cốt lõi để loại dữ liệu dư thừa: Process / File / Registry / Network.
namespace etwc::providers {

// Microsoft-Windows-Kernel-Process
inline constexpr wchar_t kKernelProcess[] = L"Microsoft-Windows-Kernel-Process";
// Microsoft-Windows-Kernel-File
inline constexpr wchar_t kKernelFile[] = L"Microsoft-Windows-Kernel-File";
// Microsoft-Windows-Kernel-Registry
inline constexpr wchar_t kKernelRegistry[] = L"Microsoft-Windows-Kernel-Registry";
// Microsoft-Windows-Kernel-Network
inline constexpr wchar_t kKernelNetwork[] = L"Microsoft-Windows-Kernel-Network";

// Keyword bitmasks (tham chiếu manifest của từng provider khi hiện thực).
enum ProcessKeyword : std::uint64_t { ProcessCreateTerm = 0x10 };
enum FileKeyword : std::uint64_t { FileReadWrite = 0x60, FileDelete = 0x100 };
enum RegistryKeyword : std::uint64_t { RegSetDelete = 0x40 };
enum NetworkKeyword : std::uint64_t { NetOutbound = 0x40 };

}  // namespace etwc::providers
