#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "etwc/common/types.hpp"

namespace etwc {

// Nhóm provider mà một sự kiện thô thuộc về (gán ngay tại callback ETW,
// vì mỗi provider có callback riêng nên biết chắc nguồn).
enum class ProviderId : std::uint8_t {
    Other = 0,
    KernelProcess,
    KernelFile,
    KernelRegistry,
    KernelNetwork,
};

// Sự kiện thô nhận trực tiếp từ callback ETW (KRABSETW), trước khi chuẩn hóa.
// Giữ các trường ở dạng gần với schema gốc; Normalizer sẽ diễn giải.
struct RawEvent {
    ProviderId provider = ProviderId::Other;
    std::uint16_t event_id = 0;
    std::uint8_t opcode = 0;
    Timestamp timestamp = 0;  // FILETIME 100ns (từ EVENT_HEADER)
    Pid pid = 0;
    Pid tid = 0;

    // Các thuộc tính đã decode được từ TDH, key = tên property (đã UTF-8).
    std::unordered_map<std::string, std::string> properties;

    std::string get(const std::string& key) const {
        auto it = properties.find(key);
        return it == properties.end() ? std::string{} : it->second;
    }

    bool has(const std::string& key) const { return properties.count(key) > 0; }
};

}  // namespace etwc
