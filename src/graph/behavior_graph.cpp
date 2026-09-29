#include "etwc/graph/behavior_graph.hpp"

namespace etwc {
namespace {
std::string make_key(EntityType type, const std::string& key) {
    return std::to_string(static_cast<int>(type)) + "|" + key;
}
const std::vector<Edge> kEmptyEdges{};
}  // namespace

VertexId BehaviorGraph::get_or_create_vertex(EntityType type, const std::string& key,
                                             const std::string& label, Timestamp ts) {
    const std::string idx = make_key(type, key);
    if (auto it = index_.find(idx); it != index_.end()) {
        Vertex& v = vertices_[it->second];
        v.last_seen = ts;
        return it->second;
    }
    VertexId id = next_id_++;
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

void BehaviorGraph::add_edge(VertexId src, VertexId dst, EventKind kind, Timestamp ts) {
    adjacency_[src].push_back(Edge{src, dst, kind, ts});
    ++edge_count_;
}

void BehaviorGraph::ingest(const NormalizedEvent& ev) {
    // Nút chủ thể (process).
    VertexId proc = get_or_create_vertex(EntityType::Process,
                                         std::to_string(ev.pid),
                                         ev.process_name, ev.timestamp);
    pid_index_[ev.pid] = proc;

    switch (ev.kind) {
        case EventKind::ProcessCreate: {
            // Liên kết nhân quả cha -> con.
            if (ev.ppid != 0) {
                VertexId parent = get_or_create_vertex(
                    EntityType::Process, std::to_string(ev.ppid),
                    ev.parent_name, ev.timestamp);
                add_edge(parent, proc, ev.kind, ev.timestamp);
            }
            break;
        }
        case EventKind::ProcessTerminate: {
            if (Vertex* v = find_vertex(proc)) v->alive = false;
            break;
        }
        case EventKind::FileRead:
        case EventKind::FileWrite:
        case EventKind::FileDelete:
        case EventKind::FileRename: {
            VertexId f = get_or_create_vertex(EntityType::File, ev.target,
                                              ev.target, ev.timestamp);
            add_edge(proc, f, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::RegSetValue:
        case EventKind::RegCreateKey:
        case EventKind::RegDeleteKey: {
            VertexId r = get_or_create_vertex(EntityType::Registry, ev.target,
                                              ev.target, ev.timestamp);
            add_edge(proc, r, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::NetConnect: {
            VertexId s = get_or_create_vertex(EntityType::Socket, ev.target,
                                              ev.target, ev.timestamp);
            add_edge(proc, s, ev.kind, ev.timestamp);
            break;
        }
        case EventKind::DnsQuery: {
            VertexId d = get_or_create_vertex(EntityType::Dns, ev.target,
                                              ev.target, ev.timestamp);
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

}  // namespace etwc
