#include "etwc/normalizer/normalizer.hpp"

#include <windows.h>
// <windows.h> định nghĩa macro RegCreateKey/RegSetValue/RegDeleteKey (A/W) đụng
// với tên enum EventKind::RegCreateKey... -> gỡ bỏ (ta không gọi các API này).
#undef RegCreateKey
#undef RegSetValue
#undef RegDeleteKey

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "etwc/common/path_normalizer.hpp"
#include "etwc/common/uuid.hpp"
#include "etwc/sensor/peb_reader.hpp"
#include "etwc/sensor/providers.hpp"

namespace etwc {
namespace {

namespace pe = providers::process_event;
namespace fe = providers::file_event;
namespace re = providers::registry_event;
namespace ne = providers::network_event;

// Lấy property đầu tiên không rỗng trong danh sách tên ứng viên.
std::string first_of(const RawEvent& raw, std::initializer_list<const char*> names) {
    for (const char* n : names) {
        std::string v = raw.get(n);
        if (!v.empty())
            return v;
    }
    return {};
}

std::uint32_t to_u32(const std::string& s) {
    if (s.empty())
        return 0;
    try {
        return static_cast<std::uint32_t>(std::stoul(s));
    } catch (...) {
        return 0;
    }
}

// Chuẩn hóa đường dẫn thiết bị -> DOS; giữ nguyên nếu không map được.
std::string to_dos(const std::string& nt_path) {
    if (nt_path.empty())
        return {};
    if (auto dos = device_path_to_dos(nt_path))
        return *dos;
    return nt_path;
}

// daddr là UInt32 đọc từ buffer (network byte order). Định dạng a.b.c.d.
std::string format_ipv4(std::uint32_t v) {
    std::array<char, 16> buf{};
    std::snprintf(buf.data(), buf.size(), "%u.%u.%u.%u", v & 0xFF, (v >> 8) & 0xFF,
                  (v >> 16) & 0xFF, (v >> 24) & 0xFF);
    return std::string(buf.data());
}

// dport là UInt16 network order đọc thành host LE -> đổi về số cổng.
std::uint16_t ntohs_port(std::uint32_t v) {
    return static_cast<std::uint16_t>(((v & 0xFF) << 8) | ((v >> 8) & 0xFF));
}

// Kiểm tra tiến trình có chạy dưới SYSTEM (S-1-5-18) không. Best-effort:
// cần quyền mở token; thất bại -> false.
bool process_is_system(Pid pid) {
    HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (hProc == nullptr)
        return false;
    bool result = false;
    HANDLE hTok = nullptr;
    if (::OpenProcessToken(hProc, TOKEN_QUERY, &hTok)) {
        DWORD len = 0;
        ::GetTokenInformation(hTok, TokenUser, nullptr, 0, &len);
        if (len > 0) {
            std::string blob(len, '\0');
            if (::GetTokenInformation(hTok, TokenUser, blob.data(), len, &len)) {
                auto* tu = reinterpret_cast<TOKEN_USER*>(blob.data());
                PSID system_sid = nullptr;
                SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
                if (::AllocateAndInitializeSid(&nt, 1, SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0, 0,
                                               0, &system_sid)) {
                    result = ::EqualSid(tu->User.Sid, system_sid) != 0;
                    ::FreeSid(system_sid);
                }
            }
        }
        ::CloseHandle(hTok);
    }
    ::CloseHandle(hProc);
    return result;
}

}  // namespace

Normalizer::Normalizer() = default;

EventKind Normalizer::classify(const RawEvent& raw) const {
    switch (raw.provider) {
        case ProviderId::KernelProcess:
            if (raw.event_id == pe::kStart)
                return EventKind::ProcessCreate;
            if (raw.event_id == pe::kStop)
                return EventKind::ProcessTerminate;
            return EventKind::Unknown;

        case ProviderId::KernelFile:
            switch (raw.event_id) {
                case fe::kRead:
                    return EventKind::FileRead;
                case fe::kWrite:
                    return EventKind::FileWrite;
                case fe::kDeletePath:
                    return EventKind::FileDelete;
                case fe::kRenamePath:
                    return EventKind::FileRename;
                case fe::kCreate:
                    // Create không có EventKind riêng nhưng mang FileName ->
                    // dùng để nạp cache tên; phân loại là FileRead (mở/đọc).
                    return EventKind::FileRead;
                default:
                    return EventKind::Unknown;
            }

        case ProviderId::KernelRegistry:
            switch (raw.event_id) {
                case re::kCreateKey:
                    return EventKind::RegCreateKey;
                case re::kSetValueKey:
                    return EventKind::RegSetValue;
                case re::kDeleteKey:
                case re::kDeleteValueKey:
                    return EventKind::RegDeleteKey;
                default:
                    return EventKind::Unknown;
            }

        case ProviderId::KernelNetwork:
            if (raw.event_id == ne::kTcpConnectV4 || raw.event_id == ne::kTcpConnectV6)
                return EventKind::NetConnect;
            return EventKind::Unknown;

        default:
            return EventKind::Unknown;
    }
}

std::optional<NormalizedEvent> Normalizer::normalize(const RawEvent& raw) {
    const EventKind kind = classify(raw);
    if (kind == EventKind::Unknown)
        return std::nullopt;

    NormalizedEvent ev;
    ev.uuid = generate_uuid_v4();
    ev.kind = kind;
    ev.timestamp = raw.timestamp;
    ev.pid = raw.pid;  // mặc định = pid trong EVENT_HEADER; ghi đè bên dưới nếu cần

    switch (raw.provider) {
        case ProviderId::KernelProcess:
            fill_process(raw, ev);
            break;
        case ProviderId::KernelFile:
            fill_file(raw, ev);
            break;
        case ProviderId::KernelRegistry:
            fill_registry(raw, ev);
            break;
        case ProviderId::KernelNetwork:
            fill_network(raw, ev);
            break;
        default:
            break;
    }

    enrich_lineage(ev);
    enrich_privilege(ev);
    enrich_command_line(ev);
    return ev;
}

void Normalizer::fill_process(const RawEvent& raw, NormalizedEvent& ev) {
    // ProcessStart: property "ProcessID" = tiến trình con (mới), EVENT_HEADER.pid
    // = tiến trình cha. ProcessStop: "ProcessID" = tiến trình đang kết thúc.
    const std::uint32_t proc_id = to_u32(first_of(raw, {"ProcessID", "ProcessId"}));
    if (proc_id != 0)
        ev.pid = proc_id;

    ev.process_name = to_dos(first_of(raw, {"ImageName", "ImageFileName"}));

    if (ev.kind == EventKind::ProcessCreate) {
        ev.ppid = to_u32(first_of(raw, {"ParentProcessID", "ParentProcessId"}));
        ev.token_elevated = to_u32(first_of(raw, {"ProcessTokenIsElevated"})) != 0;
        ev.is_system = process_is_system(ev.pid);

        // Nạp cache phả hệ.
        ProcessInfo info;
        info.name = ev.process_name;
        info.ppid = ev.ppid;
        info.is_system = ev.is_system;
        info.token_elevated = ev.token_elevated;
        process_cache_[ev.pid] = std::move(info);
    }
}

void Normalizer::fill_file(const RawEvent& raw, NormalizedEvent& ev) {
    const std::string handle = first_of(raw, {"FileObject", "FileKey"});
    std::string name = to_dos(first_of(raw, {"FileName", "FilePath"}));

    if (!name.empty()) {
        if (!handle.empty())
            file_name_by_handle_[handle] = name;  // nạp cache handle -> tên
    } else if (!handle.empty()) {
        auto it = file_name_by_handle_.find(handle);  // Read/Write: tra cache
        if (it != file_name_by_handle_.end())
            name = it->second;
    }

    ev.target = name.empty() ? ("FileObject:" + handle) : name;
}

void Normalizer::fill_registry(const RawEvent& raw, NormalizedEvent& ev) {
    std::string key = first_of(raw, {"KeyName", "RelativeName", "BaseName"});
    const std::string value = first_of(raw, {"ValueName"});
    if (!value.empty())
        key += "\\" + value;
    ev.target = key;
}

void Normalizer::fill_network(const RawEvent& raw, NormalizedEvent& ev) {
    // Sự kiện outbound: pid trong "PID"/EVENT_HEADER; địa chỉ đích daddr/dport.
    const std::uint32_t pid_prop = to_u32(first_of(raw, {"PID"}));
    if (pid_prop != 0)
        ev.pid = pid_prop;

    const std::string daddr_raw = first_of(raw, {"daddr"});
    const std::string dport_raw = first_of(raw, {"dport"});
    if (!daddr_raw.empty()) {
        ev.remote_addr = format_ipv4(to_u32(daddr_raw));  // IPv4 (event v4)
    }
    if (!dport_raw.empty())
        ev.remote_port = ntohs_port(to_u32(dport_raw));

    std::string target = ev.remote_addr.value_or("");
    if (ev.remote_port)
        target += ":" + std::to_string(*ev.remote_port);
    ev.target = target;
}

void Normalizer::enrich_lineage(NormalizedEvent& ev) {
    auto it = process_cache_.find(ev.pid);
    if (it == process_cache_.end())
        return;
    const ProcessInfo& info = it->second;
    if (ev.ppid == 0)
        ev.ppid = info.ppid;
    if (ev.process_name.empty())
        ev.process_name = info.name;

    auto pit = process_cache_.find(ev.ppid);
    if (pit != process_cache_.end() && ev.parent_name.empty())
        ev.parent_name = pit->second.name;
}

void Normalizer::enrich_privilege(NormalizedEvent& ev) {
    if (ev.kind == EventKind::ProcessCreate)
        return;  // đã set trực tiếp từ property
    auto it = process_cache_.find(ev.pid);
    if (it != process_cache_.end()) {
        ev.is_system = it->second.is_system;
        ev.token_elevated = it->second.token_elevated;
    }
}

void Normalizer::enrich_command_line(NormalizedEvent& ev) {
    if (ev.kind == EventKind::ProcessCreate && ev.command_line.empty()) {
        if (auto cmd = read_command_line_from_peb(ev.pid))
            ev.command_line = *cmd;
    }
}

}  // namespace etwc
