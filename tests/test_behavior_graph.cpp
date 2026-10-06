#include "etwc/graph/behavior_graph.hpp"

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

TEST_CASE("BehaviorGraph links parent -> child on ProcessCreate", "[graph]") {
    etwc::BehaviorGraph g;
    etwc::NormalizedEvent e = ev(etwc::EventKind::ProcessCreate, 100, 4, "child.exe", "");
    e.parent_name = "explorer.exe";
    g.ingest(e);
    REQUIRE(g.vertex_count() == 2);  // cha + con
    REQUIRE(g.edge_count() == 1);
}

TEST_CASE("BehaviorGraph builds process->file->net chain", "[graph]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "app.exe", ""));
    g.ingest(ev(etwc::EventKind::FileWrite, 100, 4, "app.exe", "C:\\a.txt"));
    g.ingest(ev(etwc::EventKind::NetConnect, 100, 4, "app.exe", "1.2.3.4:443"));
    // nút: parent(4), app(100), file, socket = 4; cạnh: parent->app, app->file, app->socket = 3
    REQUIRE(g.vertex_count() == 4);
    REQUIRE(g.edge_count() == 3);
}

TEST_CASE("BehaviorGraph dedups repeated edges into count", "[graph]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "app.exe", ""));
    for (int i = 0; i < 5; ++i)
        g.ingest(ev(etwc::EventKind::FileRead, 100, 4, "app.exe", "C:\\same.txt", 10 + i));
    // 5 lần đọc cùng file -> 1 cạnh (count=5), 1 nút file.
    REQUIRE(g.vertex_count() == 3);  // parent + app + file
    REQUIRE(g.edge_count() == 2);    // parent->app, app->file
}

TEST_CASE("BehaviorGraph handles PID reuse with new instance", "[graph]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "first.exe", "", 1));
    g.ingest(ev(etwc::EventKind::ProcessTerminate, 100, 4, "first.exe", "", 2));
    // PID 100 tái dụng: tiến trình MỚI -> nút tiến trình tách biệt.
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "second.exe", "", 3));
    // nút: parent(4), first(100), second(100-mới) = 3
    REQUIRE(g.vertex_count() == 3);
}

TEST_CASE("BehaviorGraph export DOT and JSON", "[graph]") {
    etwc::BehaviorGraph g;
    g.ingest(ev(etwc::EventKind::ProcessCreate, 100, 4, "app.exe", ""));
    g.ingest(ev(etwc::EventKind::FileWrite, 100, 4, "app.exe", "C:\\a.txt"));

    const std::string dot = g.to_dot();
    REQUIRE(dot.find("digraph provenance") != std::string::npos);
    REQUIRE(dot.find("FileWrite") != std::string::npos);

    const std::string json = g.to_json();
    REQUIRE(json.find("\"vertices\"") != std::string::npos);
    REQUIRE(json.find("\"edges\"") != std::string::npos);
    REQUIRE(json.find("FileWrite") != std::string::npos);
}
#else
int test_behavior_graph_fallback() {
    return 0;
}
#endif
