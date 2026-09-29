#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "etwc/common/types.hpp"

namespace etwc {

// Định dạng thống nhất sau lớp Normalizer (step 2).
// Mọi sự kiện thô ETW đều được đưa về struct này trước khi vào Ring Buffer.
struct NormalizedEvent {
    std::string uuid;          // UUID v4
    EventKind kind = EventKind::Unknown;
    Timestamp timestamp = 0;   // FILETIME 100ns

    // --- Chủ thể (process gây ra sự kiện) ---
    Pid pid = 0;
    Pid ppid = 0;
    std::string process_name;
    std::string parent_name;   // làm giàu bằng tra cứu ngược PPID
    std::string command_line;  // bổ sung từ PEB nếu ETW khuyết

    // --- Đối tượng bị tác động ---
    // File path / registry key / remote endpoint / dns name tùy theo kind.
    std::string target;

    // --- Nhãn đặc quyền ---
    bool is_system = false;
    bool token_elevated = false;

    // --- Network (chỉ dùng với NetConnect / DnsQuery) ---
    std::optional<std::string> remote_addr;
    std::optional<std::uint16_t> remote_port;
};

}  // namespace etwc
