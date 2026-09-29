#include "etwc/common/path_normalizer.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "etwc/common/encoding.hpp"

namespace etwc {
namespace {

std::shared_mutex g_mutex;
// Ánh xạ "\Device\HarddiskVolumeN" (thường) -> "C:". Khóa dạng lower-case.
std::vector<std::pair<std::string, std::string>> g_volume_map;
bool g_initialized = false;

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    return s;
}

// Dựng lại bảng volume. Gọi khi giữ unique_lock.
void build_map_locked() {
    g_volume_map.clear();
    const DWORD drives = ::GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(drives & (1u << i)))
            continue;
        wchar_t dos[3] = {static_cast<wchar_t>(L'A' + i), L':', L'\0'};
        wchar_t target[MAX_PATH];
        const DWORD n = ::QueryDosDeviceW(dos, target, MAX_PATH);
        if (n == 0)
            continue;
        // target ví dụ: \Device\HarddiskVolume4
        std::string device = to_lower(wide_to_utf8(target, ::wcslen(target)));
        std::string letter = wide_to_utf8(dos, 2);  // "C:"
        g_volume_map.emplace_back(std::move(device), std::move(letter));
    }
    g_initialized = true;
}

void ensure_initialized() {
    {
        std::shared_lock lock(g_mutex);
        if (g_initialized)
            return;
    }
    std::unique_lock lock(g_mutex);
    if (!g_initialized)
        build_map_locked();
}

bool starts_with(const std::string& s, const char* prefix) {
    const std::size_t n = std::strlen(prefix);
    return s.size() >= n && std::equal(prefix, prefix + n, s.begin());
}

}  // namespace

std::optional<std::string> device_path_to_dos(const std::string& nt_path) {
    if (nt_path.empty())
        return std::nullopt;

    // "\??\C:\..." -> "C:\..."
    if (starts_with(nt_path, "\\??\\"))
        return nt_path.substr(4);

    // Đã là đường dẫn DOS ("C:\...") hoặc UNC ("\\server\..."): trả nguyên trạng.
    if (nt_path.size() >= 2 && std::isalpha(static_cast<unsigned char>(nt_path[0])) &&
        nt_path[1] == ':')
        return nt_path;
    if (starts_with(nt_path, "\\\\"))
        return nt_path;

    ensure_initialized();

    // "\Device\HarddiskVolumeN\rest" -> "X:\rest".
    const std::string lower = to_lower(nt_path);
    {
        std::shared_lock lock(g_mutex);
        for (const auto& [device, letter] : g_volume_map) {
            if (lower.size() > device.size() && lower.compare(0, device.size(), device) == 0 &&
                nt_path[device.size()] == '\\') {
                return letter + nt_path.substr(device.size());
            }
        }
    }

    // Đường dẫn thiết bị không map được (named pipe, mailslot, mup...): giữ nguyên
    // để không mất thông tin, nhưng báo hiệu chưa chuẩn hóa qua nullopt.
    if (starts_with(lower, "\\device\\"))
        return std::nullopt;

    return nt_path;
}

void refresh_volume_map() {
    std::unique_lock lock(g_mutex);
    build_map_locked();
}

}  // namespace etwc
