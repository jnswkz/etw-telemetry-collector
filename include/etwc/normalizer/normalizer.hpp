#pragma once

#include <optional>

#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/sensor/raw_event.hpp"

namespace etwc {

// Step 2 — Chuẩn hóa & mở rộng ngữ cảnh.
// Chuyển RawEvent (ETW) -> NormalizedEvent: gán UUID, ánh xạ opcode,
// làm giàu parentName qua tra cứu PPID, gán nhãn đặc quyền từ token,
// bổ sung CommandLine từ PEB khi thiếu, chuẩn hóa đường dẫn.
class Normalizer {
public:
    Normalizer();

    // Trả về nullopt nếu sự kiện thô không thuộc nhóm quan tâm.
    std::optional<NormalizedEvent> normalize(const RawEvent& raw);

private:
    EventKind map_opcode(const RawEvent& raw) const;
    void enrich_lineage(NormalizedEvent& ev) const;      // parentName
    void enrich_privilege(NormalizedEvent& ev) const;    // is_system/elevated
    void enrich_command_line(NormalizedEvent& ev) const; // PEB fallback
};

}  // namespace etwc
