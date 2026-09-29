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
# Dependencies (via vcpkg manifest) — REQUIRED để lỗi lộ ngay khi configure.
# ---------------------------------------------------------------------------
# krabsetw là header-only: không có CMake config, dùng find_path.
find_path(KRABSETW_INCLUDE_DIRS "krabs.hpp" REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ETWC_BUILD_TESTS "Build unit tests" ON)
option(ETWC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Interface target mang cờ cảnh báo/chuẩn chung (tái dùng cho core + tests).
# ---------------------------------------------------------------------------
add_library(etwc_warnings INTERFACE)
if(MSVC)
    target_compile_options(etwc_warnings INTERFACE
        /W4 /permissive- /EHsc /utf-8
        # Hạ cảnh báo cho header external (SYSTEM include) xuống 0 để /WX không
        # vấp phải cảnh báo trong krabsetw/sqlite/nlohmann.
        /external:W0
        $<$<BOOL:${ETWC_WARNINGS_AS_ERRORS}>:/WX>)
endif()

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
