# Simplish — Root CMakeLists.txt
#
# Cross-platform 2D isometric game engine (C++20).
# See REQUIREMENTS.md for project overview and docs/development/ for build details.

cmake_minimum_required(VERSION 3.25)

project(simplish
    VERSION   0.1.0
    LANGUAGES CXX
    DESCRIPTION "Cross-platform 2D isometric game engine"
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Create a build directory: "
        "cmake -B build")
endif()

# Objective-C++ is required for the Metal backend on macOS.
if(APPLE)
    enable_language(OBJCXX)
endif()

# ---------------------------------------------------------------------------
# CMake utility modules
# ---------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(SimplishPlatform)
include(SimplishRenderer)
include(SimplishCompilerOptions)
include(SimplishDependencies)
include(SimplishTarget)
include(SimplishCodeQuality)

# ---------------------------------------------------------------------------
# Project options
# ---------------------------------------------------------------------------
option(ENGINE_BUILD_TESTS  "Build Catch2 test targets"      ON)
option(ENGINE_BUILD_EDITOR "Build the editor (desktop only)" ${ENGINE_PLATFORM_DESKTOP})

if(ENGINE_BUILD_EDITOR AND NOT ENGINE_PLATFORM_DESKTOP)
    message(WARNING "Editor requires a desktop platform — disabling")
    set(ENGINE_BUILD_EDITOR OFF)
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
endif()

# ---------------------------------------------------------------------------
# Source tree — all packages live under src/
# ---------------------------------------------------------------------------
add_subdirectory(src)
