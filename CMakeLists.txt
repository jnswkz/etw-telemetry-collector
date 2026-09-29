cmake_minimum_required(VERSION 3.24)

project(etw-telemetry-collector
    VERSION 0.1.0
    DESCRIPTION "ETW-based provenance telemetry collector for Windows (UNICORN-inspired)"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Global settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "This project targets Windows only (ETW / KRABSETW).")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Multi-config vs single-config output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---------------------------------------------------------------------------
# Dependencies (via vcpkg manifest)
# ---------------------------------------------------------------------------
find_package(krabs CONFIG QUIET)          # ETW consumer wrapper
find_package(unofficial-sqlite3 CONFIG QUIET)
find_package(nlohmann_json CONFIG QUIET)
find_package(stduuid CONFIG QUIET)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ETWC_BUILD_TESTS "Build unit tests" ON)

# ---------------------------------------------------------------------------
# Core library (everything except the service entry point)
# ---------------------------------------------------------------------------
add_subdirectory(src)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(ETWC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
