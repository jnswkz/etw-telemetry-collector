#pragma once

#include <optional>
#include <string>

#include "etwc/common/types.hpp"

namespace etwc {

// Bổ sung CommandLine bị khuyết do độ trễ kernel:
// đọc trực tiếp Process Environment Block (PEB) của tiến trình trong RAM.
// Trả về nullopt nếu không mở được process hoặc không có quyền.
std::optional<std::string> read_command_line_from_peb(Pid pid);

// Đọc image path đầy đủ từ PEB (fallback khi ETW thiếu ImageName).
std::optional<std::string> read_image_path_from_peb(Pid pid);

}  // namespace etwc
