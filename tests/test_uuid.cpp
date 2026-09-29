#include "etwc/common/uuid.hpp"

#ifdef ETWC_HAVE_CATCH2
#include <catch2/catch_test_macros.hpp>

TEST_CASE("UUID v4 format", "[uuid]") {
    auto id = etwc::generate_uuid_v4();
    REQUIRE(id.size() == 36);
    REQUIRE(id[14] == '4');  // version nibble
}
#else
#include <cassert>
int run_fallback_tests() {
    assert(etwc::generate_uuid_v4().size() == 36);
    return 0;
}
#endif
