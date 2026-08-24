# SimplishPlatform.cmake — Platform detection and compile definitions
#
# Sets ENGINE_PLATFORM (string) and per-platform booleans.
# Console platforms are set via toolchain file or -DENGINE_PLATFORM=PS5/XBOX_SERIES_X.
# Desktop platforms are auto-detected from CMAKE_SYSTEM_NAME.

# ---------------------------------------------------------------------------
# Auto-detect desktop platforms (only if not already set by toolchain/user)
# ---------------------------------------------------------------------------
if(NOT DEFINED ENGINE_PLATFORM OR ENGINE_PLATFORM STREQUAL "")
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(ENGINE_PLATFORM "MACOS" CACHE STRING "Target platform" FORCE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(ENGINE_PLATFORM "WINDOWS" CACHE STRING "Target platform" FORCE)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(ENGINE_PLATFORM "LINUX" CACHE STRING "Target platform" FORCE)
    else()
        message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}. "
            "Set ENGINE_PLATFORM to one of: MACOS, WINDOWS, LINUX, PS5, XBOX_SERIES_X")
    endif()
endif()

# ---------------------------------------------------------------------------
# Per-platform booleans
# ---------------------------------------------------------------------------
set(ENGINE_PLATFORM_MACOS   OFF)
set(ENGINE_PLATFORM_WINDOWS OFF)
set(ENGINE_PLATFORM_LINUX   OFF)
set(ENGINE_PLATFORM_PS5     OFF)
set(ENGINE_PLATFORM_XBOX    OFF)
set(ENGINE_PLATFORM_DESKTOP OFF)
set(ENGINE_PLATFORM_CONSOLE OFF)

if(ENGINE_PLATFORM STREQUAL "MACOS")
    set(ENGINE_PLATFORM_MACOS   ON)
    set(ENGINE_PLATFORM_DESKTOP ON)
elseif(ENGINE_PLATFORM STREQUAL "WINDOWS")
    set(ENGINE_PLATFORM_WINDOWS ON)
    set(ENGINE_PLATFORM_DESKTOP ON)
elseif(ENGINE_PLATFORM STREQUAL "LINUX")
    set(ENGINE_PLATFORM_LINUX   ON)
    set(ENGINE_PLATFORM_DESKTOP ON)
elseif(ENGINE_PLATFORM STREQUAL "PS5")
    set(ENGINE_PLATFORM_PS5     ON)
    set(ENGINE_PLATFORM_CONSOLE ON)
elseif(ENGINE_PLATFORM STREQUAL "XBOX_SERIES_X")
    set(ENGINE_PLATFORM_XBOX    ON)
    set(ENGINE_PLATFORM_CONSOLE ON)
else()
    message(FATAL_ERROR "Unknown ENGINE_PLATFORM: '${ENGINE_PLATFORM}'. "
        "Expected one of: MACOS, WINDOWS, LINUX, PS5, XBOX_SERIES_X")
endif()

# ---------------------------------------------------------------------------
# Architecture detection
# ---------------------------------------------------------------------------
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(ENGINE_ARCH_X86_64 ON)
    set(ENGINE_ARCH_ARM64  OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(ENGINE_ARCH_X86_64 OFF)
    set(ENGINE_ARCH_ARM64  ON)
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    set(ENGINE_ARCH_X86_64 OFF)
    set(ENGINE_ARCH_ARM64  OFF)
endif()

# ---------------------------------------------------------------------------
# Compile definitions list (applied to all targets via SimplishTarget)
# ---------------------------------------------------------------------------
set(ENGINE_PLATFORM_DEFINES
    ENGINE_PLATFORM_${ENGINE_PLATFORM}=1
)

if(ENGINE_PLATFORM_DESKTOP)
    list(APPEND ENGINE_PLATFORM_DEFINES ENGINE_PLATFORM_DESKTOP=1)
endif()
if(ENGINE_PLATFORM_CONSOLE)
    list(APPEND ENGINE_PLATFORM_DEFINES ENGINE_PLATFORM_CONSOLE=1)
endif()
if(ENGINE_ARCH_X86_64)
    list(APPEND ENGINE_PLATFORM_DEFINES ENGINE_ARCH_X86_64=1)
endif()
if(ENGINE_ARCH_ARM64)
    list(APPEND ENGINE_PLATFORM_DEFINES ENGINE_ARCH_ARM64=1)
endif()

# ---------------------------------------------------------------------------
# Status message
# ---------------------------------------------------------------------------
message(STATUS "Simplish platform: ${ENGINE_PLATFORM}")
message(STATUS "  Desktop: ${ENGINE_PLATFORM_DESKTOP}")
message(STATUS "  Console: ${ENGINE_PLATFORM_CONSOLE}")
message(STATUS "  Arch x86_64: ${ENGINE_ARCH_X86_64}")
message(STATUS "  Arch arm64:  ${ENGINE_ARCH_ARM64}")
