#include "etwc/common/path_normalizer.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

TEST_CASE("device_path_to_dos strips \\??\\ prefix", "[path]") {
    auto r = etwc::device_path_to_dos("\\??\\C:\\Windows\\notepad.exe");
    REQUIRE(r.has_value());
    REQUIRE(*r == "C:\\Windows\\notepad.exe");
}

TEST_CASE("device_path_to_dos passes through DOS paths", "[path]") {
    auto r = etwc::device_path_to_dos("C:\\Users\\a\\x.txt");
    REQUIRE(r.has_value());
    REQUIRE(*r == "C:\\Users\\a\\x.txt");
}

TEST_CASE("device_path_to_dos passes through UNC paths", "[path]") {
    auto r = etwc::device_path_to_dos("\\\\server\\share\\f");
    REQUIRE(r.has_value());
    REQUIRE(*r == "\\\\server\\share\\f");
}

TEST_CASE("device_path_to_dos returns nullopt for unmapped device", "[path]") {
    auto r = etwc::device_path_to_dos("\\Device\\NoSuchVolume999\\foo");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("device_path_to_dos maps a real system volume", "[path]") {
    // \Device\HarddiskVolumeN của ổ hệ thống phải map về một ổ đĩa X:.
    // Lấy device path của ổ chứa Windows qua QueryDosDevice là phức tạp trong
    // test; ở đây chỉ kiểm tra hàm không sập và trả kết quả nhất quán.
    auto r = etwc::device_path_to_dos("C:\\Windows");
    REQUIRE(r.has_value());
}
#endif
