#include "etwc/common/encoding.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

TEST_CASE("encoding round-trips ASCII and Unicode", "[encoding]") {
    const std::wstring w = L"C:\\Users\\tùng\\notepad.exe";
    const std::string u8 = etwc::wide_to_utf8(w);
    REQUIRE(!u8.empty());
    REQUIRE(etwc::utf8_to_wide(u8) == w);
}

TEST_CASE("encoding handles empty input", "[encoding]") {
    REQUIRE(etwc::wide_to_utf8(std::wstring{}).empty());
    REQUIRE(etwc::utf8_to_wide(std::string{}).empty());
    REQUIRE(etwc::wide_to_utf8(nullptr, 0).empty());
}
#endif
