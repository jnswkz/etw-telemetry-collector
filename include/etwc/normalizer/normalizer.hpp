#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

#include "etwc/normalizer/normalized_event.hpp"
#include "etwc/sensor/raw_event.hpp"

namespace etwc {

// Thông tin tiến trình lưu trong cache (dựng từ ProcessStart) để làm giàu
// phả hệ (parentName) và nhãn đặc quyền cho các sự kiện sau đó.
struct ProcessInfo {
    std::string name;             // tên/đường dẫn ảnh (đã chuẩn hóa DOS)
    Pid ppid = 0;                 // tiến trình cha
    bool is_system = false;       // chạy dưới NT AUTHORITY\SYSTEM (S-1-5-18)
    bool token_elevated = false;  // token đã nâng đặc quyền
};

// Step 2 — Chuẩn hóa & mở rộng ngữ cảnh.
// Chuyển RawEvent (ETW) -> NormalizedEvent: gán UUID, ánh xạ opcode/event-id,
// trích property theo từng loại, làm giàu parentName qua cache PID, gán nhãn
// đặc quyền, bổ sung CommandLine từ PEB khi thiếu, chuẩn hóa đường dẫn.
//
// Lưu ý: normalize() được gọi trên MỘT luồng (luồng xử lý ETW của krabs),
// nên các cache bên trong không cần khóa.
class Normalizer {
public:
    Normalizer();

    // Trả về nullopt nếu sự kiện thô không thuộc nhóm quan tâm.
    std::optional<NormalizedEvent> normalize(const RawEvent& raw);

private:
    EventKind classify(const RawEvent& raw) const;

    // Trích property đặc thù theo nhóm provider.
    void fill_process(const RawEvent& raw, NormalizedEvent& ev);
    void fill_file(const RawEvent& raw, NormalizedEvent& ev);
    void fill_registry(const RawEvent& raw, NormalizedEvent& ev);
    void fill_network(const RawEvent& raw, NormalizedEvent& ev);

    // Làm giàu chung.
    void enrich_lineage(NormalizedEvent& ev);       // parentName + ppid từ cache
    void enrich_privilege(NormalizedEvent& ev);     // is_system / token_elevated
    void enrich_command_line(NormalizedEvent& ev);  // PEB fallback (ProcessCreate)

    // pid -> thông tin tiến trình (dựng khi ProcessCreate).
    std::unordered_map<Pid, ProcessInfo> process_cache_;
    // FileObject/FileKey (chuỗi) -> đường dẫn file (DOS), để gán tên cho
    // sự kiện Read/Write vốn không mang FileName.
    std::unordered_map<std::string, std::string> file_name_by_handle_;
};

}  // namespace etwc
