#include "etwc/graph/pruner.hpp"

namespace etwc {

bool Pruner::is_long_running_system(const Vertex& v) const {
    return v.type == EntityType::Process &&
           clean_process_allowlist_.count(v.label) > 0;
}

std::size_t Pruner::purge_dead_nodes() {
    std::size_t removed = 0;
    // TODO: duyệt các process vertex alive == false, kiểm tra không còn
    //       cạnh mở (file/socket đang hoạt động) và mọi con đã chết,
    //       rồi xóa nhánh khỏi vertices_/adjacency_/index_.
    return removed;
}

std::size_t Pruner::collapse_clean_subgraphs() {
    std::size_t collapsed = 0;
    // TODO: với process hệ thống sạch chạy lâu dài, thu gọn subgraph con
    //       thành nút đại diện để giữ RAM 20–40 MB.
    return collapsed;
}

std::size_t Pruner::run_cycle() {
    return purge_dead_nodes() + collapse_clean_subgraphs();
}

}  // namespace etwc
