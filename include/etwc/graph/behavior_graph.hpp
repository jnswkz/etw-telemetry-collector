#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "etwc/common/types.hpp"
#include "etwc/normalizer/normalized_event.hpp"

namespace etwc {

using VertexId = std::uint64_t;

// Nút (thực thể) trong đồ thị nguồn gốc.
struct Vertex {
    VertexId id = 0;
    EntityType type = EntityType::Process;
    std::string key;    // định danh logic (PID, path, endpoint...)
    std::string label;  // tên hiển thị
    Timestamp first_seen = 0;
    Timestamp last_seen = 0;
    bool alive = true;            // process còn sống? (phục vụ pruning)
    std::uint64_t collapsed = 0;  // số cạnh/nút con đã bị thu gọn (clean subgraph)
};

// Cạnh (quan hệ nhân quả) — gộp các lần lặp lại thành count + last_ts.
struct Edge {
    VertexId src = 0;
    VertexId dst = 0;
    EventKind kind = EventKind::Unknown;
    Timestamp first_ts = 0;
    Timestamp last_ts = 0;
    std::uint64_t count = 0;  // số lần hành vi (src,dst,kind) lặp lại
};

// Step 4 — Đồ thị nhân quả trong bộ nhớ (adjacency list).
// Kế thừa Partial Ordering Guarantee của UNICORN: cập nhật streaming, chỉ tạo
// nút/cạnh mới và cập nhật trạng thái nút đích mà không duyệt lại toàn đồ thị.
class BehaviorGraph {
public:
    BehaviorGraph() = default;

    // Nạp một sự kiện đã chuẩn hóa, cập nhật đồ thị tăng dần.
    void ingest(const NormalizedEvent& ev);

    // Tra/khởi tạo nút tài nguyên theo (type, key). Idempotent.
    VertexId get_or_create_vertex(EntityType type, const std::string& key, const std::string& label,
                                  Timestamp ts);

    // Thêm/gộp cạnh (src,dst,kind). Lặp lại -> tăng count, cập nhật last_ts.
    void add_edge(VertexId src, VertexId dst, EventKind kind, Timestamp ts);

    std::size_t vertex_count() const { return vertices_.size(); }
    std::size_t edge_count() const { return edge_count_; }

    // Truy cập cho Pruner (step 5).
    Vertex* find_vertex(VertexId id);
    const std::vector<Edge>& out_edges(VertexId id) const;

    // Xóa một tập nút (và mọi cạnh chạm tới chúng), dựng lại chỉ mục. Trả về số
    // nút thực sự bị xóa. Dùng bởi Pruner.
    std::size_t remove_vertices(const std::unordered_set<VertexId>& ids);

    // Xuất đồ thị để trực quan hóa / điều tra.
    std::string to_dot() const;   // Graphviz
    std::string to_json() const;  // {vertices:[...], edges:[...]}

private:
    friend class Pruner;

    // Mỗi ProcessCreate tạo nút tiến trình MỚI (xử lý PID tái dụng); các sự kiện
    // khác tra nút tiến trình hiện hành của pid.
    VertexId new_process_vertex(Pid pid, const std::string& name, Timestamp ts);
    VertexId resolve_process(Pid pid, const std::string& name, Timestamp ts);

    std::unordered_map<VertexId, Vertex> vertices_;
    std::unordered_map<VertexId, std::vector<Edge>> adjacency_;  // adjacency list
    std::unordered_map<std::string, VertexId> index_;            // (type|key) -> id (tài nguyên)
    std::unordered_map<std::string, std::size_t> edge_index_;    // (src|dst|kind) -> vị trí
    std::unordered_map<Pid, VertexId> pid_index_;                // PID -> nút tiến trình hiện hành
    VertexId next_id_ = 1;
    std::size_t edge_count_ = 0;
};

}  // namespace etwc
