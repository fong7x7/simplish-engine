# SimplishRenderer.cmake — Select GPU backend at configure time
#
# Sets ENGINE_RENDERER_RESOLVED and appends ENGINE_RENDERER_<NAME>=1 for the
# primary backend (METAL, VULKAN, DX12, OPENGL, PS5). No secondary renderer
# macros: OpenGL TUs compile only when the primary pick is OPENGL.

set(ENGINE_RENDERER "AUTO" CACHE STRING
    "GPU backend: AUTO (platform default), METAL, VULKAN, DX12, OPENGL, PS5, STUB")
set_property(CACHE ENGINE_RENDERER PROPERTY STRINGS
    AUTO METAL VULKAN DX12 OPENGL PS5 STUB)

if(ENGINE_RENDERER STREQUAL "" OR ENGINE_RENDERER STREQUAL "AUTO")
    if(ENGINE_PLATFORM_MACOS)
        set(_simplish_renderer_pick "METAL")
    elseif(ENGINE_PLATFORM_WINDOWS OR ENGINE_PLATFORM_XBOX)
        set(_simplish_renderer_pick "DX12")
    elseif(ENGINE_PLATFORM_LINUX)
        set(_simplish_renderer_pick "VULKAN")
    elseif(ENGINE_PLATFORM_PS5)
        set(_simplish_renderer_pick "PS5")
    else()
        message(FATAL_ERROR "ENGINE_RENDERER=AUTO: unknown ENGINE_PLATFORM")
    endif()
else()
    set(_simplish_renderer_pick "${ENGINE_RENDERER}")
endif()

# STUB is the headless backend: no GPU, no windowing, valid on every platform.
# It is the determinism/CI path — see docs/decisions/ADR-006-headless-deterministic-ci.md.
if(_simplish_renderer_pick STREQUAL "METAL" AND NOT ENGINE_PLATFORM_MACOS)
    message(FATAL_ERROR
        "ENGINE_RENDERER=METAL requires macOS (ENGINE_PLATFORM=MACOS).")
endif()
if(_simplish_renderer_pick STREQUAL "PS5" AND NOT ENGINE_PLATFORM_PS5)
    message(FATAL_ERROR
        "ENGINE_RENDERER=PS5 requires ENGINE_PLATFORM=PS5.")
endif()
if(_simplish_renderer_pick STREQUAL "DX12")
    if(NOT (ENGINE_PLATFORM_WINDOWS OR ENGINE_PLATFORM_XBOX))
        message(FATAL_ERROR
            "ENGINE_RENDERER=DX12 requires Windows or Xbox Series X.")
    endif()
endif()
if(_simplish_renderer_pick STREQUAL "VULKAN" AND NOT ENGINE_PLATFORM_DESKTOP)
    message(FATAL_ERROR
        "ENGINE_RENDERER=VULKAN requires a desktop platform.")
endif()
if(_simplish_renderer_pick STREQUAL "OPENGL" AND NOT ENGINE_PLATFORM_DESKTOP)
    message(FATAL_ERROR
        "ENGINE_RENDERER=OPENGL requires a desktop platform.")
endif()

set(ENGINE_RENDERER_RESOLVED "${_simplish_renderer_pick}" CACHE INTERNAL
    "Graphics API after AUTO resolution" FORCE)

list(APPEND ENGINE_PLATFORM_DEFINES
    "ENGINE_RENDERER_${_simplish_renderer_pick}=1")

message(STATUS "Simplish renderer: ${ENGINE_RENDERER_RESOLVED} "
    "(cache ENGINE_RENDERER=${ENGINE_RENDERER})")
