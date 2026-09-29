#pragma once

#include <string>

namespace etwc {

// Chuyển đổi giữa UTF-16 (Windows wide) và UTF-8. Dùng cho tên tiến trình,
// đường dẫn, command line... vốn là wide string trong ETW/PEB.
std::string wide_to_utf8(const wchar_t* s, std::size_t len);
std::string wide_to_utf8(const std::wstring& s);
std::wstring utf8_to_wide(const std::string& s);

}  // namespace etwc
