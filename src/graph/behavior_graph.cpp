#include "etwc/graph/behavior_graph.hpp"

#include <iterator>
#include <utility>

namespace etwc {
namespace {
std::string resource_key(EntityType type, const std::string& key) {
    return std::to_string(static_cast<int>(type)) + "|" + key;
}
std::string edge_key(VertexId src, VertexId dst, EventKind kind) {
    return std::to_string(src) + "|" + std::to_string(dst) + "|" +
           std::to_string(static_cast<int>(kind));
}
const std::vector<Edge> kEmptyEdges{};
}  // namespace

VertexId BehaviorGraph::get_or_create_vertex(EntityType type, const std::string& key,
                                             const std::string& label, Timestamp ts) {
    const std::string idx = resource_key(type, key);
    if (auto it = index_.find(idx); it != index_.end()) {
        Vertex& v = vertices_[it->second];
        v.last_seen = ts;
        return it->second;
    }
    const VertexId id = next_id_++;
    Vertex v;
    v.id = id;
    v.type = type;
    v.key = key;
    v.label = label;
    v.first_seen = ts;
    v.last_seen = ts;
    vertices_.emplace(id, std::move(v));
    index_.emplace(idx, id);
    return id;
}

VertexId BehaviorGraph::new_process_vertex(Pid pid, const std::string& name, Timestamp ts) {
    const VertexId id = next_id_++;
    Vertex v;
    v.id = id;
    v.type = EntityType::Process;
    v.key = std::to_string(pid);
    v.label = name.empty() ? ("pid:" + std::to_string(pid)) : name;
    v.first_seen = ts;
    v.last_seen = ts;
    vertices_.emplace(id, std::move(v));
    pid_index_[pid] = id;  // nút hiện hành cho pid (ghi đè instance cũ nếu tái dụng)
    return id;
}

VertexId BehaviorGraph::resolve_process(Pid pid, const std::string& name, Timestamp ts) {
    if (auto it = pid_index_.find(pid); it != pid_index_.end()) {
        Vertex& v = vertices_[it->second];
        v.last_seen = ts;
        if (v.label.empty() && !name.empty())
            v.label = name;
        return it->second;
    }
    return new_process_vertex(pid, name, ts);
}

void BehaviorGraph::add_edge(VertexId src, VertexId dst, EventKind kind, Timestamp ts) {
    const std::string ek = edge_key(src, dst, kind);
    if (auto it = edge_index_.find(ek); it != edge_index_.end()) {
        Edge& e = adjacency_[src][it->second];
        e.last_ts = ts;
        ++e.count;
        return;
    }
    std::vector<Edge>& bucket = adjacency_[src];
    edge_index_.emplace(ek, bucket.size());
    bucket.push_back(Edge{src, dst, kind, ts, ts, 1});
    ++edge_count_;
}

void BehaviorGraph::ingest(const NormalizedEvent& ev) {
    switch (ev.kind) {
        case EventKind::ProcessCreate: {
            // Tạo nút tiến trình con MỚI (xử lý PID tái dụng) + nối cha->con.
            const VertexId child = new_process_vertex(ev.pid, ev.process_name, ev.timestamp);
            if (ev.ppid != 0) {
                const VertexId parent = resolve_process(ev.ppid, ev.parent_name, ev.timestamp);
                add_edge(parent, child, ev.kind, ev.timestamp);
            }
            return;
        }
        case EventKind::ProcessTerminate: {
            if (auto it = pid_index_.find(ev.pid); it != pid_index_.end()) {
                if (Vertex* v = find_vertex(it->second))
                    v->alive = false;
            }
            return;
        }
        default:
            break;
    }

    // Sự kiện tài nguyên: chủ thể là tiến trình hiện hành của pid.
    const VertexId proc = resolve_process(ev.pid, ev.process_name, ev.timestamp);

    switch (ev.kind) {
        case EventKind::FileRead:
        case EventKind::FileWrite:
        case EventKind::FileDelete:
        case EventKind::FileRename: {
            const VertexId f =
                get_or_create_vertex(EntityType::File, ev.target, ev.target, ev.timestamp);
            add_edge(proc, f, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::RegSetValue:
        case EventKind::RegCreateKey:
        case EventKind::RegDeleteKey: {
            const VertexId r =
                get_or_create_vertex(EntityType::Registry, ev.target, ev.target, ev.timestamp);
            add_edge(proc, r, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::NetConnect: {
            const VertexId s =
                get_or_create_vertex(EntityType::Socket, ev.target, ev.target, ev.timestamp);
            add_edge(proc, s, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::DnsQuery: {
            const VertexId d =
                get_or_create_vertex(EntityType::Dns, ev.target, ev.target, ev.timestamp);
            add_edge(proc, d, ev.kind, ev.timestamp);
            break;
        }
        default:
            break;
    }
}

Vertex* BehaviorGraph::find_vertex(VertexId id) {
    auto it = vertices_.find(id);
    return it == vertices_.end() ? nullptr : &it->second;
}

const std::vector<Edge>& BehaviorGraph::out_edges(VertexId id) const {
    auto it = adjacency_.find(id);
    return it == adjacency_.end() ? kEmptyEdges : it->second;
}

std::size_t BehaviorGraph::remove_vertices(const std::unordered_set<VertexId>& ids) {
    if (ids.empty())
        return 0;

    std::size_t removed = 0;
    for (VertexId id : ids) removed += vertices_.erase(id);

    // Dựng lại adjacency_ + edge_index_ + edge_count_, bỏ cạnh chạm nút đã xóa.
    std::unordered_map<VertexId, std::vector<Edge>> new_adj;
    std::unordered_map<std::string, std::size_t> new_eidx;
    std::size_t new_ecount = 0;
    for (auto& [src, edges] : adjacency_) {
        if (ids.count(src))
            continue;
        std::vector<Edge> kept;
        for (const Edge& e : edges) {
            if (ids.count(e.dst))
                continue;
            new_eidx.emplace(edge_key(e.src, e.dst, e.kind), kept.size());
            kept.push_back(e);
        }
        if (!kept.empty()) {
            new_ecount += kept.size();
            new_adj.emplace(src, std::move(kept));
        }
    }
    adjacency_ = std::move(new_adj);
    edge_index_ = std::move(new_eidx);
    edge_count_ = new_ecount;

    // Dọn chỉ mục tài nguyên và pid.
    for (auto it = index_.begin(); it != index_.end();)
        it = ids.count(it->second) ? index_.erase(it) : std::next(it);
    for (auto it = pid_index_.begin(); it != pid_index_.end();)
        it = ids.count(it->second) ? pid_index_.erase(it) : std::next(it);

    return removed;
}

}  // namespace etwc
