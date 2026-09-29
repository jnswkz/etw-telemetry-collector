#include "etwc/common/types.hpp"

namespace etwc {

const char* to_string(EventKind kind) noexcept {
    switch (kind) {
        case EventKind::ProcessCreate:    return "ProcessCreate";
        case EventKind::ProcessTerminate: return "ProcessTerminate";
        case EventKind::FileRead:         return "FileRead";
        case EventKind::FileWrite:        return "FileWrite";
        case EventKind::FileDelete:       return "FileDelete";
        case EventKind::FileRename:       return "FileRename";
        case EventKind::RegSetValue:      return "RegSetValue";
        case EventKind::RegCreateKey:     return "RegCreateKey";
        case EventKind::RegDeleteKey:     return "RegDeleteKey";
        case EventKind::NetConnect:       return "NetConnect";
        case EventKind::DnsQuery:         return "DnsQuery";
        default:                          return "Unknown";
    }
}

const char* to_string(EntityType type) noexcept {
    switch (type) {
        case EntityType::Process:  return "Process";
        case EntityType::File:     return "File";
        case EntityType::Registry: return "Registry";
        case EntityType::Socket:   return "Socket";
        case EntityType::Dns:      return "Dns";
        default:                   return "Unknown";
    }
}

}  // namespace etwc
