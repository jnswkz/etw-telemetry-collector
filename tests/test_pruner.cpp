#include "etwc/graph/behavior_graph.hpp"
#include "etwc/graph/pruner.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

namespace {
etwc::NormalizedEvent ev(etwc::EventKind kind, etwc::Pid pid, etwc::Pid ppid, const char* pname,
                         const char* target, etwc::Timestamp ts = 1) {
    etwc::NormalizedEvent e;
    e.kind = kind;
    e.pid = pid;
    e.ppid = ppid;
    e.process_name = pname;
    e.target = target;
    e.timestamp = ts;
    return e;
}
}  // namespace

TEST_CASE("Pruner removes dead process and its orphaned resource", "[pruner]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "app.exe", "", 1));
    g.ingest(ev(etwc::EventKind::FileWrite, 100, 4, "app.exe", "C:\\a.txt", 2));
    g.ingest(ev(etwc::EventKind::ProcessTerminate, 100, 4, "app.exe", "", 3));
    // Trước prune: parent(4) + app(100) + file = 3 nút.
    REQUIRE(g.vertex_count() == 3);

    etwc::Pruner pruner(g);
    const std::size_t removed = pruner.purge_dead_nodes();
    REQUIRE(removed >= 2);  // app chết + file mồ côi
    // Còn lại tiến trình cha (còn sống).
    REQUIRE(g.vertex_count() == 1);
}

TEST_CASE("Pruner keeps dead parent with alive child", "[pruner]") {
    etwc::BehaviorGraph g;
    // parent 100 tạo child 200, rồi parent chết nhưng child còn sống.
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "parent.exe", "", 1));
    g.ingest(ev(etwc::EventKind::ProcessCreate, 200, 100, "child.exe", "", 2));
    g.ingest(ev(etwc::EventKind::ProcessTerminate, 100, 4, "parent.exe", "", 3));

    etwc::Pruner pruner(g);
    pruner.purge_dead_nodes();
    // parent(100) được giữ vì có con sống -> grandparent(4)+parent(100)+child(200)=3.
    REQUIRE(g.vertex_count() == 3);
}

TEST_CASE("Pruner keeps resource shared with alive process", "[pruner]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "dead.exe", "", 1));
    g.ingest(ev(etwc::EventKind::ProcessCreate, 200, 4, "live.exe", "", 2));
    g.ingest(ev(etwc::EventKind::FileWrite, 100, 4, "dead.exe", "C:\\shared.txt", 3));
    g.ingest(ev(etwc::EventKind::FileWrite, 200, 4, "live.exe", "C:\\shared.txt", 4));
    g.ingest(ev(etwc::EventKind::ProcessTerminate, 100, 4, "dead.exe", "", 5));

    etwc::Pruner pruner(g);
    pruner.purge_dead_nodes();
    // dead(100) bị xóa; file dùng chung còn được live(200) trỏ -> giữ.
    // Còn: parent(4) + live(200) + shared.txt = 3.
    REQUIRE(g.vertex_count() == 3);
}

TEST_CASE("Pruner collapses clean long-running process subtree", "[pruner]") {
    etwc::BehaviorGraph g;
    // Tiến trình hệ thống sạch, chạy lâu, ghi 200 file khác nhau.
    g.ingest(ev(etwc::EventKind::ProcessCreate, 900, 4, "C:\\Windows\\svchost.exe", "", 1));
    for (int i = 0; i < 200; ++i)
        g.ingest(ev(etwc::EventKind::FileWrite, 900, 4, "C:\\Windows\\svchost.exe",
                    ("C:\\tmp\\f" + std::to_string(i) + ".dat").c_str(),
                    static_cast<etwc::Timestamp>(10 + i)));
    const std::size_t before = g.vertex_count();

    etwc::Pruner pruner(g);
    const std::size_t removed = pruner.collapse_clean_subgraphs();
    REQUIRE(removed > 0);                     // đã thu gọn bớt tài nguyên cũ
    REQUIRE(g.vertex_count() < before);       // RAM giảm
    REQUIRE(g.vertex_count() <= 2 + 64 + 1);  // svchost + cap 64 tài nguyên + parent
}
#endif
