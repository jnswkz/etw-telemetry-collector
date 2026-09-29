#pragma once

#include <cstddef>
#include <unordered_set>

#include "etwc/graph/behavior_graph.hpp"

namespace etwc {

// Step 5 — Cắt tỉa đồ thị tự động, giữ RAM của collector ở mức 20–40 MB.
class Pruner {
public:
    explicit Pruner(BehaviorGraph& graph) : graph_(graph) {}

    // Thu hồi nút chết: process đã PROCESS_TERMINATE, con cũng đã chết,
    // và không còn kết nối nhân quả mở (file/socket) -> giải phóng cả nhánh.
    std::size_t purge_dead_nodes();

    // Thu gọn nút an toàn: process hệ thống sạch, chạy lâu dài
    // (explorer.exe, services.exe...) -> collapse subgraph con.
    std::size_t collapse_clean_subgraphs();

    // Gọi định kỳ (theo Config::prune_interval_ms) từ luồng consumer.
    std::size_t run_cycle();

private:
    bool is_long_running_system(const Vertex& v) const;

    BehaviorGraph& graph_;
    std::unordered_set<std::string> clean_process_allowlist_ = {
        "explorer.exe", "services.exe", "svchost.exe",  "lsass.exe",
        "csrss.exe",    "wininit.exe",  "winlogon.exe",
    };
};

}  // namespace etwc
