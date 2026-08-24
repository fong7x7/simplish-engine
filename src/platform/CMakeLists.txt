# simplish-platform — every platform-specific implementation
#
# This is the only layer that recompiles per target or per distribution.
# It links simplish-engine; the engine never links back.
#
#   bin/  ──►  platform/  ──►  engine/
#
# Building for a different distribution (Steam, Epic, a console) or a
# different RHI backend rebuilds this library only — engine object files
# stay valid. See docs/platform/REQUIREMENTS.md.

add_subdirectory(render)

if(ENGINE_PLATFORM_DESKTOP)
    add_subdirectory(client)
endif()

set(_platform_modules simplish-platform-render)

# Exactly one backend is compiled in, chosen by SimplishRenderer.cmake.
if(ENGINE_PLATFORM_MACOS AND ENGINE_RENDERER_RESOLVED STREQUAL "METAL")
    list(APPEND _platform_modules simplish-platform-render-backend-metal)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "OPENGL")
    list(APPEND _platform_modules simplish-platform-render-backend-opengl)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "VULKAN")
    list(APPEND _platform_modules simplish-platform-render-backend-vulkan)
elseif(ENGINE_PLATFORM_WINDOWS AND ENGINE_RENDERER_RESOLVED STREQUAL "DX12")
    list(APPEND _platform_modules simplish-platform-render-backend-dx12)
else()
    list(APPEND _platform_modules simplish-platform-render-backend-stub)
endif()

if(ENGINE_PLATFORM_DESKTOP)
    list(APPEND _platform_modules simplish-platform-client)
endif()

simplish_add_library(simplish-platform
    TYPE    STATIC
    MODULES ${_platform_modules}
)

target_link_libraries(simplish-platform PUBLIC simplish-engine)

if(ENGINE_PLATFORM_DESKTOP)
    target_link_libraries(simplish-platform PUBLIC SDL3::SDL3)
endif()
