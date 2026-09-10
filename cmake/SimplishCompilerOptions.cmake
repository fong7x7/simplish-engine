# SimplishCompilerOptions.cmake — Compiler flags, warnings, sanitizers, LTO
#
# Creates an INTERFACE library `simplish_compiler_options` that all targets link.
# Requires SimplishPlatform.cmake to be included first.

add_library(simplish_compiler_options INTERFACE)
add_library(simplish::compiler_options ALIAS simplish_compiler_options)

# ---------------------------------------------------------------------------
# C++23 (required for std::expected used in error handling)
# ---------------------------------------------------------------------------
target_compile_features(simplish_compiler_options INTERFACE cxx_std_23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Warning flags
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(simplish_compiler_options INTERFACE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus          # Report correct __cplusplus value
        /utf-8                   # Source and execution charset UTF-8
    )
    # Standards-conforming preprocessor. clang-cl's always is, and rejects the
    # flag as unused — an error under /WX.
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(simplish_compiler_options INTERFACE
            /Zc:preprocessor)
    endif()
    # The CRT deprecates standard functions such as std::getenv in favour of
    # its own _s variants; portable code keeps the standard ones.
    target_compile_definitions(simplish_compiler_options INTERFACE
        _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(simplish_compiler_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wcast-align
        -Wformat=2
        -Wimplicit-fallthrough
        -Wmissing-include-dirs
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Woverloaded-virtual
        -Wshadow
        -Wunused
    )
endif()

# ---------------------------------------------------------------------------
# Floating point — the determinism contract (ADR-002)
# ---------------------------------------------------------------------------
# Clang contracts a*b+c into a fused multiply-add by default. arm64 always has
# FMA and x86_64 here does not (-mavx2 without -mfma), so the same expression
# rounds differently on the two architectures and a tick hash diverges. Off
# for every target, so simulation code cannot inherit the default by accident.
#
# clang-cl sets MSVC but is not MSVC here: under /fp:precise it still
# contracts, and its /arch:AVX2 turns FMA on, so it takes the Clang flags
# (spelled /clang: to reach the driver) rather than /fp:precise.
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(simplish_compiler_options INTERFACE /fp:precise)
elseif(MSVC)
    target_compile_options(simplish_compiler_options INTERFACE
        /clang:-ffp-contract=off
        /clang:-fno-fast-math
    )
else()
    target_compile_options(simplish_compiler_options INTERFACE
        -ffp-contract=off
        -fno-fast-math
    )
endif()

# ---------------------------------------------------------------------------
# SIMD flags
# ---------------------------------------------------------------------------
# x86_64: enable SSE4.2 + AVX2 (per REQUIREMENTS.md §3)
# arm64 (macOS, PS5): NEON is enabled by default, no flag needed
if(ENGINE_ARCH_X86_64 AND NOT MSVC)
    target_compile_options(simplish_compiler_options INTERFACE
        -msse4.2
        -mavx2
    )
elseif(ENGINE_ARCH_X86_64 AND MSVC)
    target_compile_options(simplish_compiler_options INTERFACE
        /arch:AVX2
    )
endif()

# ---------------------------------------------------------------------------
# Build-type-specific flags
# ---------------------------------------------------------------------------
# Debug: assertions enabled, no optimisation
target_compile_definitions(simplish_compiler_options INTERFACE
    $<$<CONFIG:Debug>:ENGINE_DEBUG=1>
    $<$<CONFIG:RelWithDebInfo>:ENGINE_DEBUG=1>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:NDEBUG>
)

# ---------------------------------------------------------------------------
# Shipping mode
# ---------------------------------------------------------------------------
option(ENGINE_SHIPPING "Enable shipping build (strips debug, full LTO)" OFF)
if(ENGINE_SHIPPING)
    target_compile_definitions(simplish_compiler_options INTERFACE ENGINE_SHIPPING=1)
endif()

# ---------------------------------------------------------------------------
# Link-Time Optimisation (LTO)
# ---------------------------------------------------------------------------
# Enabled for Release and Shipping builds
include(CheckIPOSupported)
check_ipo_supported(RESULT ENGINE_IPO_SUPPORTED OUTPUT ENGINE_IPO_ERROR)
if(ENGINE_IPO_SUPPORTED)
    message(STATUS "IPO/LTO: supported")
else()
    message(STATUS "IPO/LTO: not supported (${ENGINE_IPO_ERROR})")
endif()

# LTO is applied per-target in SimplishTarget.cmake based on build type

# ---------------------------------------------------------------------------
# Coverage (opt-in, for llvm-cov)
# ---------------------------------------------------------------------------
option(ENGINE_COVERAGE "Enable coverage instrumentation (Clang llvm-cov)" OFF)
if(ENGINE_COVERAGE AND NOT MSVC)
    target_compile_options(simplish_compiler_options INTERFACE
        $<$<CONFIG:Debug>:-fprofile-instr-generate -fcoverage-mapping>
        $<$<CONFIG:RelWithDebInfo>:-fprofile-instr-generate -fcoverage-mapping>
    )
    target_link_options(simplish_compiler_options INTERFACE
        $<$<CONFIG:Debug>:-fprofile-instr-generate>
        $<$<CONFIG:RelWithDebInfo>:-fprofile-instr-generate>
    )
    message(STATUS "Coverage: enabled (Debug/RelWithDebInfo)")
endif()

# ---------------------------------------------------------------------------
# Sanitizers (opt-in, Debug only)
# ---------------------------------------------------------------------------
option(ENGINE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer in Debug" OFF)
if(ENGINE_SANITIZERS)
    if(NOT MSVC)
        target_compile_options(simplish_compiler_options INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
            $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        )
        target_link_options(simplish_compiler_options INTERFACE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    else()
        target_compile_options(simplish_compiler_options INTERFACE
            $<$<CONFIG:Debug>:/fsanitize=address>
        )
    endif()
    message(STATUS "Sanitizers: enabled (Debug only)")
endif()

# ---------------------------------------------------------------------------
# Compile commands (for clang-tidy)
# ---------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json" FORCE)
