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

# Link dependencies if found (guarded so the tree configures even before vcpkg install)
if(TARGET krabs::krabs)
    target_link_libraries(etwc_core PUBLIC krabs::krabs)
endif()
if(TARGET unofficial::sqlite3::sqlite3)
    target_link_libraries(etwc_core PUBLIC unofficial::sqlite3::sqlite3)
endif()
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(etwc_core PUBLIC nlohmann_json::nlohmann_json)
endif()
if(TARGET stduuid)
    target_link_libraries(etwc_core PUBLIC stduuid)
endif()

# Windows system libs used by the sensor / service layers
target_link_libraries(etwc_core PUBLIC
    tdh          # Trace Data Helper (ETW schema decoding)
    advapi32     # Service control + tokens
    ws2_32       # Network address helpers
    ntdll        # PEB access
)

# ---------------------------------------------------------------------------
# etwcollector : the Windows Service executable (entry point only)
# ---------------------------------------------------------------------------
add_executable(etwcollector
    service/service_main.cpp
    service/service_control.cpp
    main.cpp
)

target_link_libraries(etwcollector PRIVATE etwc_core)
set_target_properties(etwcollector PROPERTIES OUTPUT_NAME "etwcollector")
