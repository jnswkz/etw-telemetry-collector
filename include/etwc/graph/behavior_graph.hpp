#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "etwc/common/types.hpp"
#include "etwc/normalizer/normalized_event.hpp"

namespace etwc {

using VertexId = std::uint64_t;

// Nút (thực thể) trong đồ thị nguồn gốc.
struct Vertex {
    VertexId id = 0;
    EntityType type = EntityType::Process;
    std::string key;          // định danh logic (PID, path, endpoint...)
    std::string label;        // tên hiển thị
    Timestamp first_seen = 0;
    Timestamp last_seen = 0;
    bool alive = true;        // process còn sống? (phục vụ pruning)
};

// Cạnh (quan hệ nhân quả) — mang nhãn hành vi và thời điểm.
struct Edge {
    VertexId src = 0;
    VertexId dst = 0;
    EventKind kind = EventKind::Unknown;
    Timestamp ts = 0;
};

// Step 4 — Đồ thị nhân quả trong bộ nhớ (adjacency list).
// Kế thừa Partial Ordering Guarantee của UNICORN: cập nhật streaming,
// chỉ tạo nút/cạnh mới và cập nhật nút đích, không duyệt lại toàn đồ thị.
class BehaviorGraph {
public:
    BehaviorGraph() = default;

    // Nạp một sự kiện đã chuẩn hóa, cập nhật đồ thị tăng dần.
    void ingest(const NormalizedEvent& ev);

    // Tra/khởi tạo nút theo (type, key). Idempotent.
    VertexId get_or_create_vertex(EntityType type, const std::string& key,
                                  const std::string& label, Timestamp ts);

    void add_edge(VertexId src, VertexId dst, EventKind kind, Timestamp ts);

    std::size_t vertex_count() const { return vertices_.size(); }
    std::size_t edge_count() const { return edge_count_; }

    // Truy cập cho Pruner (step 5).
    Vertex* find_vertex(VertexId id);
    const std::vector<Edge>& out_edges(VertexId id) const;

private:
    friend class Pruner;

    std::unordered_map<VertexId, Vertex> vertices_;
    std::unordered_map<VertexId, std::vector<Edge>> adjacency_;  // adjacency list
    std::unordered_map<std::string, VertexId> index_;           // (type|key) -> id
    std::unordered_map<Pid, VertexId> pid_index_;               // PID -> process vertex
    VertexId next_id_ = 1;
    std::size_t edge_count_ = 0;
};

}  // namespace etwc
