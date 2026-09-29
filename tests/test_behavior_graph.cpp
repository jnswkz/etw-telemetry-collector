#include "etwc/graph/behavior_graph.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

TEST_CASE("BehaviorGraph links parent -> child on ProcessCreate", "[graph]") {
    etwc::BehaviorGraph g;
    etwc::NormalizedEvent ev;
    ev.kind = etwc::EventKind::ProcessCreate;
    ev.pid = 100;
    ev.ppid = 4;
    ev.process_name = "child.exe";
    ev.parent_name = "explorer.exe";
    g.ingest(ev);

    // 1 nút con + 1 nút cha, 1 cạnh nhân quả.
    REQUIRE(g.vertex_count() == 2);
    REQUIRE(g.edge_count() == 1);
}
#else
int test_behavior_graph_fallback() {
    return 0;
}
#endif
