#include "etwc/graph/pruner.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace etwc {

bool Pruner::is_long_running_system(const Vertex& v) const {
    if (v.type != EntityType::Process)
        return false;
    std::string name = v.label;
    if (auto pos = name.find_last_of("\\/"); pos != std::string::npos)
        name = name.substr(pos + 1);
    for (auto& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return clean_process_allowlist_.count(name) > 0;
}

std::size_t Pruner::purge_dead_nodes() {
    std::unordered_set<VertexId> remove;

    // 1. Tiến trình đã chết, và không còn con tiến trình nào còn sống.
    for (const auto& [id, v] : graph_.vertices_) {
        if (v.type != EntityType::Process || v.alive)
            continue;
        bool alive_child = false;
        for (const Edge& e : graph_.out_edges(id)) {
            if (e.kind != EventKind::ProcessCreate)
                continue;
            const Vertex* c = graph_.find_vertex(e.dst);
            if (c && c->type == EntityType::Process && c->alive) {
                alive_child = true;
                break;
            }
        }
        if (!alive_child)
            remove.insert(id);
    }

    // 2. Nút tài nguyên mồ côi: không còn tiến trình SỐNG SÓT nào trỏ tới.
    std::unordered_set<VertexId> referenced;
    for (const auto& [src, edges] : graph_.adjacency_) {
        if (remove.count(src))
            continue;
        for (const Edge& e : edges) referenced.insert(e.dst);
    }
    for (const auto& [id, v] : graph_.vertices_) {
        if (v.type == EntityType::Process)
            continue;
        if (!referenced.count(id))
            remove.insert(id);
    }

    return graph_.remove_vertices(remove);
}

std::size_t Pruner::collapse_clean_subgraphs() {
    constexpr std::size_t kCleanEdgeCap = 64;  // mỗi tiến trình sạch giữ tối đa N tài nguyên

    // Bản đồ tài nguyên -> các tiến trình nguồn (in-edges).
    std::unordered_map<VertexId, std::vector<VertexId>> sources;
    for (const auto& [src, edges] : graph_.adjacency_)
        for (const Edge& e : edges) sources[e.dst].push_back(src);

    auto all_sources_clean = [&](VertexId res) {
        auto it = sources.find(res);
        if (it == sources.end())
            return true;
        for (VertexId s : it->second) {
            const Vertex* sv = graph_.find_vertex(s);
            if (sv == nullptr || !is_long_running_system(*sv))
                return false;
        }
        return true;
    };

    std::unordered_set<VertexId> remove;
    for (auto& [id, v] : graph_.vertices_) {
        if (!is_long_running_system(v) || !v.alive)
            continue;

        // Thu thập cạnh tới tài nguyên, sắp theo last_ts (cũ trước).
        std::vector<const Edge*> res_edges;
        for (const Edge& e : graph_.out_edges(id)) {
            const Vertex* dv = graph_.find_vertex(e.dst);
            if (dv && dv->type != EntityType::Process)
                res_edges.push_back(&e);
        }
        if (res_edges.size() <= kCleanEdgeCap)
            continue;

        std::sort(res_edges.begin(), res_edges.end(),
                  [](const Edge* a, const Edge* b) { return a->last_ts < b->last_ts; });
        const std::size_t to_drop = res_edges.size() - kCleanEdgeCap;
        std::uint64_t dropped = 0;
        for (std::size_t i = 0; i < res_edges.size() && dropped < to_drop; ++i) {
            const VertexId r = res_edges[i]->dst;
            if (all_sources_clean(r)) {  // chỉ gỡ nếu không tiến trình "bẩn" nào dùng
                remove.insert(r);
                ++dropped;
            }
        }
        v.collapsed += dropped;
    }

    return graph_.remove_vertices(remove);
}

std::size_t Pruner::run_cycle() {
    return purge_dead_nodes() + collapse_clean_subgraphs();
}

}  // namespace etwc
