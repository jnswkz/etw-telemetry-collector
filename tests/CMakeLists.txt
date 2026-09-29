find_package(Catch2 3 CONFIG QUIET)

add_executable(etwc_tests
    test_main.cpp
    test_ring_buffer.cpp
    test_behavior_graph.cpp
    test_uuid.cpp
)

target_link_libraries(etwc_tests PRIVATE etwc_core)

if(TARGET Catch2::Catch2WithMain)
    target_link_libraries(etwc_tests PRIVATE Catch2::Catch2WithMain)
    target_compile_definitions(etwc_tests PRIVATE ETWC_HAVE_CATCH2)
    include(Catch)
    catch_discover_tests(etwc_tests)
else()
    # Fallback: các test viết bằng assert vẫn chạy qua CTest.
    add_test(NAME etwc_tests COMMAND etwc_tests)
endif()
