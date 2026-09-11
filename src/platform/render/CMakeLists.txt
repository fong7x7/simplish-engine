# simplish-platform-render — RhiDeviceFactory and the concrete RHI backends
#
# The abstract RhiDevice / RhiCommandList interface lives in the engine
# (src/engine/render/). This package holds the implementations and the factory
# that selects among them, so a backend change never rebuilds the engine.
#
# Backend headers are included only by rhi-device-factory.cpp, keeping
# graphics SDK headers out of every other translation unit.

set(_factory_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/rhi-device-factory.cpp"
)

simplish_add_module(simplish-platform-render
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${_factory_sources}
    DEPENDENCIES simplish-engine-core simplish-engine-render
)

# --- Stub backend (always built; the headless / CI path, ADR-006) ---
file(GLOB_RECURSE _stub_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/stub/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/backends/stub/include/*.h"
)
simplish_add_module(simplish-platform-render-backend-stub
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/backends/stub/include
    SOURCES     ${_stub_sources}
    DEPENDENCIES simplish-engine-core simplish-engine-render simplish-engine-image stb::stb
)

# --- Metal backend ---
if(ENGINE_PLATFORM_MACOS AND ENGINE_RENDERER_RESOLVED STREQUAL "METAL")
    set(_metal_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-device-impl.mm
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-module.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-format-map.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-resource-map.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-stub-device.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-type-converters.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/include/engine/render/backends/metal/metal-rhi-command-list.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/include/engine/render/backends/metal/metal-rhi-config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/include/engine/render/backends/metal/metal-rhi-device.h
    )
    simplish_add_module(simplish-platform-render-backend-metal
        INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/include
        SOURCES     ${_metal_sources}
        DEPENDENCIES simplish-engine-core simplish-engine-render simplish-engine-image stb::stb
    )
    target_link_libraries(simplish-platform-render-backend-metal PUBLIC
        SDL3::SDL3
        "-framework Metal"
        "-framework MetalKit"
        "-framework QuartzCore"
        "-framework AppKit"
    )
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/metal/src/metal-device-impl.mm"
        PROPERTIES COMPILE_OPTIONS -fobjc-arc)
endif()

# --- OpenGL backend ---
if(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "OPENGL")
    file(GLOB_RECURSE _ogl_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/include/*.h"
    )
    simplish_add_module(simplish-platform-render-backend-opengl
        INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/backends/opengl/include
        SOURCES     ${_ogl_sources}
        DEPENDENCIES simplish-engine-core simplish-engine-render simplish-engine-image SDL3::SDL3 stb::stb
    )
endif()

# --- Vulkan backend ---
if(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "VULKAN")
    find_package(Vulkan REQUIRED)
    set(_vulkan_src ${CMAKE_CURRENT_SOURCE_DIR}/backends/vulkan/src)
    set(_vulkan_sources
        ${_vulkan_src}/vulkan-builtin-pipelines.cpp
        ${_vulkan_src}/vulkan-command-list.cpp
        ${_vulkan_src}/vulkan-device.cpp
        ${_vulkan_src}/vulkan-device-init.cpp
        ${_vulkan_src}/vulkan-glsl-compiler.cpp
        ${_vulkan_src}/vulkan-image-transition.cpp
        ${_vulkan_src}/vulkan-memory-allocator.cpp
        ${_vulkan_src}/vulkan-shared-layout.cpp
        ${_vulkan_src}/vulkan-upload-ring.cpp
        ${_vulkan_src}/vulkan-buffer-resource.h
        ${_vulkan_src}/vulkan-builtin-pipelines.h
        ${_vulkan_src}/vulkan-command-list.h
        ${_vulkan_src}/vulkan-device-impl.h
        ${_vulkan_src}/vulkan-format-map.h
        ${_vulkan_src}/vulkan-frames-in-flight.h
        ${_vulkan_src}/vulkan-glsl-compiler.h
        ${_vulkan_src}/vulkan-handle-table.h
        ${_vulkan_src}/vulkan-image-ref.h
        ${_vulkan_src}/vulkan-image-transition.h
        ${_vulkan_src}/vulkan-pass-attachments.h
        ${_vulkan_src}/vulkan-per-frame-data.h
        ${_vulkan_src}/vulkan-pipeline-resource.h
        ${_vulkan_src}/vulkan-retired-object.h
        ${_vulkan_src}/vulkan-shader-resource.h
        ${_vulkan_src}/vulkan-shared-layout.h
        ${_vulkan_src}/vulkan-stage-bindings.h
        ${_vulkan_src}/vulkan-texture-resource.h
        ${_vulkan_src}/vulkan-upload-ring.h
        ${CMAKE_CURRENT_SOURCE_DIR}/backends/vulkan/include/engine/render/backends/vulkan/vulkan-device.h
    )
    simplish_add_module(simplish-platform-render-backend-vulkan
        INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/backends/vulkan/include
        SOURCES     ${_vulkan_sources}
        DEPENDENCIES simplish-engine-core simplish-engine-render simplish-engine-image SDL3::SDL3 stb::stb
    )
    # glslang backs the built-in pipelines, which compile their GLSL at
    # device creation the way the Metal backend compiles its MSL.
    target_link_libraries(simplish-platform-render-backend-vulkan PUBLIC
        Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator
        glslang::glslang glslang::glslang-default-resource-limits)
endif()

# --- DX12 backend ---
if(ENGINE_PLATFORM_WINDOWS AND ENGINE_RENDERER_RESOLVED STREQUAL "DX12")
    file(GLOB_RECURSE _dx12_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/dx12/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/dx12/src/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/backends/dx12/include/*.h"
    )
    simplish_add_module(simplish-platform-render-backend-dx12
        INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/backends/dx12/include
        SOURCES     ${_dx12_sources}
        DEPENDENCIES simplish-engine-core simplish-engine-render simplish-engine-image SDL3::SDL3 stb::stb
    )
    # d3dcompiler backs the built-in GUI and mesh pipelines, which compile
    # their HLSL at device creation the way the Metal backend compiles its MSL.
    target_link_libraries(simplish-platform-render-backend-dx12 PUBLIC
        d3d12 dxgi dxguid d3dcompiler D3D12MemoryAllocator)
endif()

# rhi-device-factory.cpp conditionally includes the active backend's header;
# propagate that backend's include directory to the factory module.
if(ENGINE_PLATFORM_MACOS AND ENGINE_RENDERER_RESOLVED STREQUAL "METAL")
    set(_active_backend simplish-platform-render-backend-metal)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "OPENGL")
    set(_active_backend simplish-platform-render-backend-opengl)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "VULKAN")
    set(_active_backend simplish-platform-render-backend-vulkan)
elseif(ENGINE_PLATFORM_WINDOWS AND ENGINE_RENDERER_RESOLVED STREQUAL "DX12")
    set(_active_backend simplish-platform-render-backend-dx12)
else()
    set(_active_backend simplish-platform-render-backend-stub)
endif()
target_link_libraries(simplish-platform-render PUBLIC ${_active_backend})

# ---------------------------------------------------------------------------
# Tests — backend suites are excluded when their renderer is not selected.
# ---------------------------------------------------------------------------
file(GLOB _render_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/test/support/*.cpp")

macro(_drop_tests _cond_met _glob)
    if(NOT ${_cond_met})
        file(GLOB _drop ${_glob})
        if(_drop)
            list(REMOVE_ITEM _render_tests ${_drop})
        endif()
    endif()
endmacro()

set(_is_metal FALSE)
set(_is_opengl FALSE)
set(_is_vulkan FALSE)
set(_is_dx12 FALSE)
if(ENGINE_PLATFORM_MACOS AND ENGINE_RENDERER_RESOLVED STREQUAL "METAL")
    set(_is_metal TRUE)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "OPENGL")
    set(_is_opengl TRUE)
elseif(ENGINE_PLATFORM_DESKTOP AND ENGINE_RENDERER_RESOLVED STREQUAL "VULKAN")
    set(_is_vulkan TRUE)
elseif(ENGINE_PLATFORM_WINDOWS AND ENGINE_RENDERER_RESOLVED STREQUAL "DX12")
    set(_is_dx12 TRUE)
endif()

_drop_tests(_is_metal  "${CMAKE_CURRENT_SOURCE_DIR}/test/test_metal*.cpp;${CMAKE_CURRENT_SOURCE_DIR}/test/support/metal_*.cpp")
_drop_tests(_is_opengl "${CMAKE_CURRENT_SOURCE_DIR}/test/test_opengl*.cpp")
_drop_tests(_is_vulkan "${CMAKE_CURRENT_SOURCE_DIR}/test/test_vulkan*.cpp")
_drop_tests(_is_dx12   "${CMAKE_CURRENT_SOURCE_DIR}/test/test_dx12*.cpp")

# test_gpu_* run on whichever backend can draw — Metal, Vulkan or DX12 — on a
# hidden SDL window, and skip where no device answers. The stub and OpenGL
# builds have nothing for them to draw with.
set(_has_gpu_tests FALSE)
if(_is_metal OR _is_vulkan OR _is_dx12)
    set(_has_gpu_tests TRUE)
endif()
_drop_tests(_has_gpu_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/test_gpu*.cpp;${CMAKE_CURRENT_SOURCE_DIR}/test/support/gpu_*.cpp")

if(ENGINE_BUILD_TESTS AND _render_tests)
    set(_render_test_deps simplish-platform-render)
    # The GPU tests draw through the real GUI renderer.
    if(_has_gpu_tests)
        list(APPEND _render_test_deps simplish-engine-gui)
    endif()
    simplish_add_test(simplish-platform-render-tests
        SOURCES      ${_render_tests}
        DEPENDENCIES ${_render_test_deps}
    )
    # Backend tests reach their backend's private headers by name, as a
    # package's own tests may (code-layout.md §4).
    if(_is_dx12)
        target_include_directories(simplish-platform-render-tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/backends/dx12/src)
    endif()
    if(_is_vulkan)
        target_include_directories(simplish-platform-render-tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/backends/vulkan/src)
    endif()
endif()
