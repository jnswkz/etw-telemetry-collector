#pragma once

#include <cstdint>
#include <string>

// Core enums and shared value types for the whole pipeline.
namespace etwc {

// Normalized behavior labels, decoupled from raw ETW Event ID / Opcode.
enum class EventKind : std::uint8_t {
    Unknown = 0,
    ProcessCreate,
    ProcessTerminate,
    FileRead,
    FileWrite,
    FileDelete,
    FileRename,
    RegSetValue,
    RegCreateKey,
    RegDeleteKey,
    NetConnect,  // outbound socket
    DnsQuery,
};

// Vertex categories in the provenance graph (entities).
enum class EntityType : std::uint8_t {
    Process = 0,
    File,
    Registry,
    Socket,
    Dns,
};

using Pid = std::uint32_t;
using Timestamp = std::uint64_t;  // 100ns FILETIME ticks (ETW native)

const char* to_string(EventKind kind) noexcept;
const char* to_string(EntityType type) noexcept;

}  // namespace etwc
