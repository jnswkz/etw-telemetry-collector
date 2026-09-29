#include "etwc/common/encoding.hpp"

#include <windows.h>

namespace etwc {

std::string wide_to_utf8(const wchar_t* s, std::size_t len) {
    if (s == nullptr || len == 0)
        return {};
    const int need =
        ::WideCharToMultiByte(CP_UTF8, 0, s, static_cast<int>(len), nullptr, 0, nullptr, nullptr);
    if (need <= 0)
        return {};
    std::string out(static_cast<std::size_t>(need), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s, static_cast<int>(len), out.data(), need, nullptr, nullptr);
    return out;
}

std::string wide_to_utf8(const std::wstring& s) {
    return wide_to_utf8(s.c_str(), s.size());
}

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty())
        return {};
    const int need =
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (need <= 0)
        return {};
    std::wstring out(static_cast<std::size_t>(need), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), need);
    return out;
}

}  // namespace etwc
