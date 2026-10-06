#include <nlohmann/json.hpp>
#include <string>

#include "etwc/graph/behavior_graph.hpp"

namespace etwc {
namespace {

// Thoát ký tự cho label trong DOT (\\ và ").
std::string dot_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"')
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

const char* node_color(EntityType t) {
    switch (t) {
        case EntityType::Process:
            return "lightblue";
        case EntityType::File:
            return "lightyellow";
        case EntityType::Registry:
            return "lightgreen";
        case EntityType::Socket:
            return "salmon";
        case EntityType::Dns:
            return "orange";
        default:
            return "white";
    }
}

}  // namespace

std::string BehaviorGraph::to_dot() const {
    std::string out = "digraph provenance {\n  rankdir=LR;\n  node [style=filled];\n";
    for (const auto& [id, v] : vertices_) {
        out += "  \"" + std::to_string(id) + "\" [label=\"" + dot_escape(v.label) + "\\n(" +
               to_string(v.type) + ")\", fillcolor=" + node_color(v.type);
        if (v.type == EntityType::Process && !v.alive)
            out += ", style=\"filled,dashed\"";
        out += "];\n";
    }
    for (const auto& [src, edges] : adjacency_) {
        for (const Edge& e : edges) {
            out += "  \"" + std::to_string(e.src) + "\" -> \"" + std::to_string(e.dst) +
                   "\" [label=\"" + to_string(e.kind);
            if (e.count > 1)
                out += " x" + std::to_string(e.count);
            out += "\"];\n";
        }
    }
    out += "}\n";
    return out;
}

std::string BehaviorGraph::to_json() const {
    nlohmann::json j;
    j["vertices"] = nlohmann::json::array();
    for (const auto& [id, v] : vertices_) {
        j["vertices"].push_back({{"id", id},
                                 {"type", to_string(v.type)},
                                 {"label", v.label},
                                 {"key", v.key},
                                 {"alive", v.alive},
                                 {"first_seen", v.first_seen},
                                 {"last_seen", v.last_seen}});
    }
    j["edges"] = nlohmann::json::array();
    for (const auto& [src, edges] : adjacency_) {
        for (const Edge& e : edges) {
            j["edges"].push_back({{"src", e.src},
                                  {"dst", e.dst},
                                  {"kind", to_string(e.kind)},
                                  {"count", e.count},
                                  {"first_ts", e.first_ts},
                                  {"last_ts", e.last_ts}});
        }
    }
    return j.dump(2);
}

}  // namespace etwc
