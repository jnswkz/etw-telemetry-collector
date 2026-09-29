#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "etwc/common/types.hpp"

namespace etwc {

// Sự kiện thô nhận trực tiếp từ callback ETW (KRABSETW), trước khi chuẩn hóa.
// Giữ các trường ở dạng gần với schema gốc; Normalizer sẽ diễn giải.
struct RawEvent {
    std::uint16_t provider_id = 0;
    std::uint8_t opcode = 0;
    std::uint16_t event_id = 0;
    Timestamp timestamp = 0;
    Pid pid = 0;
    Pid tid = 0;

    // Các thuộc tính đã decode được từ TDH, key = tên property.
    std::unordered_map<std::string, std::string> properties;

    std::string get(const std::string& key) const {
        auto it = properties.find(key);
        return it == properties.end() ? std::string{} : it->second;
    }
};

}  // namespace etwc
