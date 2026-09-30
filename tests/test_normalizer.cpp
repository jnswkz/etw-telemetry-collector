#include "etwc/normalizer/normalizer.hpp"
#include "etwc/sensor/providers.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

namespace {

// Dựng RawEvent giả để test Normalizer mà không cần ETW/admin.
etwc::RawEvent make_raw(etwc::ProviderId provider, std::uint16_t event_id, etwc::Pid header_pid,
                        std::initializer_list<std::pair<const char*, const char*>> props) {
    etwc::RawEvent raw;
    raw.provider = provider;
    raw.event_id = event_id;
    raw.pid = header_pid;
    raw.timestamp = 1;
    for (auto& [k, v] : props) raw.properties.emplace(k, v);
    return raw;
}

}  // namespace

TEST_CASE("classify: out-of-scope event returns nullopt", "[normalizer]") {
    etwc::Normalizer n;
    auto ev = n.normalize(make_raw(etwc::ProviderId::KernelProcess, 99, 4, {}));
    REQUIRE_FALSE(ev.has_value());
}

TEST_CASE("ProcessCreate maps pid ppid name elevated", "[normalizer]") {
    etwc::Normalizer n;
    auto ev = n.normalize(make_raw(etwc::ProviderId::KernelProcess,
                                   etwc::providers::process_event::kStart, 4,
                                   {{"ProcessID", "1234"},
                                    {"ParentProcessID", "4"},
                                    {"ImageName", "C:\\Windows\\notepad.exe"},
                                    {"ProcessTokenIsElevated", "1"}}));
    REQUIRE(ev.has_value());
    REQUIRE(ev->kind == etwc::EventKind::ProcessCreate);
    REQUIRE(ev->pid == 1234);
    REQUIRE(ev->ppid == 4);
    REQUIRE(ev->process_name == "C:\\Windows\\notepad.exe");
    REQUIRE(ev->token_elevated == true);
    REQUIRE(!ev->uuid.empty());
}

TEST_CASE("Lineage fills parent_name from cache", "[normalizer]") {
    etwc::Normalizer n;
    // Cha trước.
    n.normalize(make_raw(etwc::ProviderId::KernelProcess, etwc::providers::process_event::kStart, 0,
                         {{"ProcessID", "4"}, {"ImageName", "C:\\Windows\\explorer.exe"}}));
    // Con.
    auto child = n.normalize(
        make_raw(etwc::ProviderId::KernelProcess, etwc::providers::process_event::kStart, 4,
                 {{"ProcessID", "1234"}, {"ParentProcessID", "4"}, {"ImageName", "C:\\app.exe"}}));
    REQUIRE(child.has_value());
    REQUIRE(child->parent_name == "C:\\Windows\\explorer.exe");
}

TEST_CASE("Registry SetValue: target = key\\value", "[normalizer]") {
    etwc::Normalizer n;
    auto ev = n.normalize(
        make_raw(etwc::ProviderId::KernelRegistry, etwc::providers::registry_event::kSetValueKey,
                 500, {{"KeyName", "\\REGISTRY\\MACHINE\\SOFTWARE\\Foo"}, {"ValueName", "Bar"}}));
    REQUIRE(ev.has_value());
    REQUIRE(ev->kind == etwc::EventKind::RegSetValue);
    REQUIRE(ev->target == "\\REGISTRY\\MACHINE\\SOFTWARE\\Foo\\Bar");
}

TEST_CASE("Network formats daddr dport to ipv4 port", "[normalizer]") {
    etwc::Normalizer n;
    // 1.2.3.4 network order đọc LE = 0x04030201 = 67305985.
    // cổng 443 network order đọc LE = 0xBB01 = 47873.
    auto ev = n.normalize(make_raw(etwc::ProviderId::KernelNetwork,
                                   etwc::providers::network_event::kTcpConnectV4, 800,
                                   {{"daddr", "67305985"}, {"dport", "47873"}}));
    REQUIRE(ev.has_value());
    REQUIRE(ev->kind == etwc::EventKind::NetConnect);
    REQUIRE(ev->remote_addr.has_value());
    REQUIRE(*ev->remote_addr == "1.2.3.4");
    REQUIRE(ev->remote_port.has_value());
    REQUIRE(*ev->remote_port == 443);
    REQUIRE(ev->target == "1.2.3.4:443");
}

TEST_CASE("File Read resolves name from FileObject cache", "[normalizer]") {
    etwc::Normalizer n;
    // Create mang FileName -> nạp cache theo FileObject.
    n.normalize(make_raw(etwc::ProviderId::KernelFile, etwc::providers::file_event::kCreate, 700,
                         {{"FileObject", "0xAB12"}, {"FileName", "C:\\data\\a.txt"}}));
    // Read không mang tên -> tra cache.
    auto rd = n.normalize(make_raw(etwc::ProviderId::KernelFile, etwc::providers::file_event::kRead,
                                   700, {{"FileObject", "0xAB12"}}));
    REQUIRE(rd.has_value());
    REQUIRE(rd->kind == etwc::EventKind::FileRead);
    REQUIRE(rd->target == "C:\\data\\a.txt");
}
#endif
