#include "etwc/sensor/peb_reader.hpp"

#include <windows.h>
// <winternl.h> phải đứng sau <windows.h>.
#include <winternl.h>

#include "etwc/common/encoding.hpp"
#include "etwc/common/handle.hpp"

#pragma comment(lib, "ntdll.lib")

namespace etwc {
namespace {

// Mở tiến trình với quyền tối thiểu để đọc PEB (RAII).
UniqueHandle open_for_read(Pid pid) {
    return UniqueHandle(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE,
                                      static_cast<DWORD>(pid)));
}

// Đọc con trỏ PEB của tiến trình đích.
PVOID query_peb_base(HANDLE h) {
    PROCESS_BASIC_INFORMATION pbi{};
    ULONG ret = 0;
    const NTSTATUS st =
        ::NtQueryInformationProcess(h, ProcessBasicInformation, &pbi, sizeof(pbi), &ret);
    if (st < 0)
        return nullptr;
    return pbi.PebBaseAddress;
}

// Đọc nội dung một UNICODE_STRING nằm trong không gian địa chỉ tiến trình đích.
std::wstring read_remote_ustring(HANDLE h, const UNICODE_STRING& us) {
    if (us.Buffer == nullptr || us.Length == 0)
        return {};
    std::wstring buf(us.Length / sizeof(wchar_t), L'\0');
    SIZE_T read = 0;
    if (!::ReadProcessMemory(h, us.Buffer, buf.data(), us.Length, &read))
        return {};
    buf.resize(read / sizeof(wchar_t));
    return buf;
}

// Đọc RTL_USER_PROCESS_PARAMETERS của tiến trình đích.
// Trả về false nếu bất kỳ bước ReadProcessMemory nào thất bại.
bool read_process_parameters(HANDLE h, RTL_USER_PROCESS_PARAMETERS& out) {
    PVOID peb_base = query_peb_base(h);
    if (peb_base == nullptr)
        return false;

    PEB peb{};
    if (!::ReadProcessMemory(h, peb_base, &peb, sizeof(peb), nullptr))
        return false;
    if (peb.ProcessParameters == nullptr)
        return false;

    return ::ReadProcessMemory(h, peb.ProcessParameters, &out, sizeof(out), nullptr) != 0;
}

}  // namespace

std::optional<std::string> read_command_line_from_peb(Pid pid) {
    UniqueHandle h = open_for_read(pid);
    if (!h)
        return std::nullopt;

    RTL_USER_PROCESS_PARAMETERS params{};
    if (read_process_parameters(h.get(), params)) {
        std::wstring cmd = read_remote_ustring(h.get(), params.CommandLine);
        if (!cmd.empty())
            return wide_to_utf8(cmd);
    }
    return std::nullopt;
}

std::optional<std::string> read_image_path_from_peb(Pid pid) {
    UniqueHandle h = open_for_read(pid);
    if (!h)
        return std::nullopt;

    RTL_USER_PROCESS_PARAMETERS params{};
    if (read_process_parameters(h.get(), params)) {
        std::wstring img = read_remote_ustring(h.get(), params.ImagePathName);
        if (!img.empty())
            return wide_to_utf8(img);
    }
    return std::nullopt;
}

}  // namespace etwc
