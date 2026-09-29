# ---------------------------------------------------------------------------
# etwc_core : static library holding the pipeline (steps 1-5)
# ---------------------------------------------------------------------------
add_library(etwc_core STATIC
    common/types.cpp
    common/config.cpp
    common/logging.cpp
    common/uuid.cpp
    common/path_normalizer.cpp

    sensor/etw_session.cpp
    sensor/providers.cpp
    sensor/peb_reader.cpp

    normalizer/normalizer.cpp

    buffer/ring_buffer.cpp

    storage/sqlite_store.cpp

    graph/behavior_graph.cpp
    graph/pruner.cpp

    service/collector.cpp
)

target_include_directories(etwc_core
    PUBLIC ${CMAKE_SOURCE_DIR}/include
)

target_compile_definitions(etwc_core PUBLIC
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

# vcpkg dependencies (find_package/find_path REQUIRED ở CMakeLists.txt gốc).
# krabsetw header-only: chỉ cần include dir.
target_include_directories(etwc_core PUBLIC ${KRABSETW_INCLUDE_DIRS})
target_link_libraries(etwc_core PUBLIC
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
    stduuid
)

# Windows system libs used by the sensor / service layers
target_link_libraries(etwc_core PUBLIC
    tdh          # Trace Data Helper (ETW schema decoding)
    advapi32     # Service control + tokens
    ws2_32       # Network address helpers
    ntdll        # PEB access
)

# Cờ cảnh báo/chuẩn chung.
target_link_libraries(etwc_core PRIVATE etwc_warnings)

# ---------------------------------------------------------------------------
# etwcollector : the Windows Service executable (entry point only)
# ---------------------------------------------------------------------------
add_executable(etwcollector
    service/service_main.cpp
    service/service_control.cpp
    main.cpp
)

target_link_libraries(etwcollector PRIVATE etwc_core etwc_warnings)
set_target_properties(etwcollector PROPERTIES OUTPUT_NAME "etwcollector")
