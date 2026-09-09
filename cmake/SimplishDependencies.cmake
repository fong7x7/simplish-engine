# SimplishDependencies.cmake — FetchContent declarations for all dependencies
#
# All dependencies are version-pinned to exact tags or commit SHAs.
# Conditional dependencies are gated by ENGINE_PLATFORM_* variables.
# Requires SimplishPlatform.cmake to be included first.

include(FetchContent)

option(ENGINE_FETCH_DEPENDENCIES "Fetch dependencies via FetchContent" ON)
option(ENGINE_ENABLE_DLSS "Enable NVIDIA DLSS support (Windows/Linux x86_64 only)" OFF)

if(NOT ENGINE_FETCH_DEPENDENCIES)
    message(STATUS "FetchContent disabled — expecting pre-installed dependencies")
else()

# ---------------------------------------------------------------------------
# Core dependencies (all platforms)
# ---------------------------------------------------------------------------

# nlohmann/json — JSON parsing for data-driven content and config loaders
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# Catch2 — testing framework
set(CATCH_CONFIG_CPP17_BYTE OFF CACHE BOOL "Enable std::byte StringMaker in Catch2" FORCE)
set(CATCH_CONFIG_CPP17_STRING_VIEW ON CACHE BOOL "Enable std::string_view StringMaker in Catch2" FORCE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)

# stb — single-header image load/write (GUI image loader, RHI capture API)
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

# FreeType — glyph rasterisation for the GUI text pipeline.
# HarfBuzz discovery is disabled: FreeType optionally uses HarfBuzz for
# auto-hint shaping, and enabling that direction creates a circular
# FetchContent dependency. The engine does not shape complex scripts.
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-3
    GIT_SHALLOW    TRUE
)

# ---------------------------------------------------------------------------
# Desktop-only dependencies
# ---------------------------------------------------------------------------
if(ENGINE_PLATFORM_DESKTOP)
    # SDL3 — windowing and input
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.2.8
        GIT_SHALLOW    TRUE
    )
endif()

# ---------------------------------------------------------------------------
# Windows-only dependencies
# ---------------------------------------------------------------------------
if(ENGINE_PLATFORM_WINDOWS)
    # D3D12MemoryAllocator — GPU heap suballocation for the DX12 backend.
    # The backend allocates every buffer and texture through it, so it is a
    # hard requirement of that backend rather than an optimisation.
    set(D3D12MA_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(D3D12MemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator.git
        GIT_TAG        v2.1.0
        GIT_SHALLOW    TRUE
    )
endif()

# ---------------------------------------------------------------------------
# Make all declared dependencies available
# ---------------------------------------------------------------------------
message(STATUS "Fetching dependencies...")

FetchContent_MakeAvailable(nlohmann_json Catch2)

set(CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz TRUE)
FetchContent_MakeAvailable(freetype)
set(CMAKE_DISABLE_FIND_PACKAGE_HarfBuzz FALSE)

if(ENGINE_PLATFORM_DESKTOP)
    # SDL3 emits deprecation warnings under our warning level; quiet them for
    # the dependency build only, then restore project flags.
    set(_saved_c_flags   "${CMAKE_C_FLAGS}")
    set(_saved_cxx_flags "${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wno-deprecated-declarations")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

    FetchContent_MakeAvailable(SDL3)

    set(CMAKE_C_FLAGS   "${_saved_c_flags}")
    set(CMAKE_CXX_FLAGS "${_saved_cxx_flags}")
endif()

if(ENGINE_PLATFORM_WINDOWS)
    FetchContent_MakeAvailable(D3D12MemoryAllocator)
endif()

# stb — header-only, no CMakeLists.txt. Populate and create INTERFACE target.
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_MakeAvailable(stb)
endif()
if(NOT TARGET stb_headers)
    add_library(stb_headers INTERFACE)
    target_include_directories(stb_headers INTERFACE "${stb_SOURCE_DIR}")
    add_library(stb::stb ALIAS stb_headers)
endif()

message(STATUS "Dependencies fetched")

endif() # ENGINE_FETCH_DEPENDENCIES
