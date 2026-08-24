# SimplishTarget.cmake — Reusable target utility functions
#
# Provides simplish_add_library(), simplish_add_executable(), simplish_add_test(),
# and simplish_add_module() that apply project-wide settings to every target.
# Requires SimplishPlatform.cmake and SimplishCompilerOptions.cmake.
#
# Module layout convention (include/src split):
#
#   engine/
#     core/
#       include/engine/core/   # Public headers (consumed by other targets)
#       src/                   # Private headers + .cpp files
#       CMakeLists.txt
#     audio/
#       include/engine/audio/
#       src/
#       CMakeLists.txt
#
# Public headers use: #include <engine/core/engine.h>
# Private headers use: #include "detail/internal.h"

# ---------------------------------------------------------------------------
# simplish_add_module(<name>
#     [INCLUDE_DIR <dir>]        # Public include root (default: include/)
#     [SRC_DIR <dir>]            # Source directory (default: src/)
#     [SOURCES <file>...]        # Explicit source list (overrides SRC_DIR glob)
#     [PUBLIC_HEADERS <file>...] # Explicit header list (overrides INCLUDE_DIR glob)
#     [DEPENDENCIES <target>...] # Link dependencies
# )
#
# Creates an OBJECT library for a subsystem module. OBJECT libraries:
#   - Compile once, link into both the parent STATIC lib and test executables
#   - Enable fast incremental test builds (no re-archiving the full static lib)
#   - No export macros needed (unlike SHARED)
#   - Release/Shipping builds compose all OBJECT libs into a single STATIC archive
#
# The module's public headers are in include/ (e.g. include/engine/core/engine.h).
# Implementation files are in src/ (e.g. src/engine.cpp, src/detail/internal.h).
#
# Tests link the OBJECT library directly:
#   target_link_libraries(my-test PRIVATE simplish-engine-core)
# ---------------------------------------------------------------------------
function(simplish_add_module name)
    cmake_parse_arguments(ARG "" "INCLUDE_DIR;SRC_DIR" "SOURCES;PUBLIC_HEADERS;DEPENDENCIES" ${ARGN})

    # Defaults
    if(NOT ARG_INCLUDE_DIR)
        set(ARG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif()
    if(NOT ARG_SRC_DIR)
        set(ARG_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()

    # Collect sources if not explicitly provided
    if(NOT ARG_SOURCES)
        file(GLOB_RECURSE ARG_SOURCES
            "${ARG_SRC_DIR}/*.cpp"
            "${ARG_SRC_DIR}/*.h"
        )
    endif()

    # Collect public headers if not explicitly provided
    if(NOT ARG_PUBLIC_HEADERS)
        file(GLOB_RECURSE ARG_PUBLIC_HEADERS
            "${ARG_INCLUDE_DIR}/*.h"
        )
    endif()

    # Guard: OBJECT libraries require at least one source file.
    # Early-stage modules that have headers but no .cpp files yet use an
    # INTERFACE library instead, which carries include dirs and dependencies
    # but produces no object code.
    set(_all_module_files ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})
    set(_has_compiled_source FALSE)
    foreach(_f ${ARG_SOURCES})
        if(_f MATCHES "\\.(cpp|cxx|cc|c)$")
            set(_has_compiled_source TRUE)
            break()
        endif()
    endforeach()

    if(NOT _has_compiled_source)
        # No compiled sources — create INTERFACE library instead of OBJECT.
        # This lets other targets depend on this module's headers and
        # transitive deps while the implementation is still being written.
        add_library(${name} INTERFACE)
        target_link_libraries(${name} INTERFACE simplish_compiler_options)
        target_compile_definitions(${name} INTERFACE ${ENGINE_PLATFORM_DEFINES})
        target_include_directories(${name} INTERFACE
            $<BUILD_INTERFACE:${ARG_INCLUDE_DIR}>
        )
        if(ARG_DEPENDENCIES)
            target_link_libraries(${name} INTERFACE ${ARG_DEPENDENCIES})
        endif()
        add_library(simplish::${name} ALIAS ${name})
        return()
    endif()

    # Create OBJECT library
    add_library(${name} OBJECT ${ARG_SOURCES} ${ARG_PUBLIC_HEADERS})

    target_link_libraries(${name} PUBLIC simplish_compiler_options)
    target_compile_definitions(${name} PUBLIC ${ENGINE_PLATFORM_DEFINES})

    # Public include: include/ directory (for consumers and the module itself)
    target_include_directories(${name} PUBLIC
        $<BUILD_INTERFACE:${ARG_INCLUDE_DIR}>
    )

    # Private include: src/ directory (for internal headers)
    target_include_directories(${name} PRIVATE
        ${ARG_SRC_DIR}
    )

    # Link dependencies
    if(ARG_DEPENDENCIES)
        target_link_libraries(${name} PUBLIC ${ARG_DEPENDENCIES})
    endif()

    # Create namespaced alias
    add_library(simplish::${name} ALIAS ${name})
endfunction()

# ---------------------------------------------------------------------------
# simplish_add_library(<name>
#     TYPE <STATIC|SHARED|INTERFACE|OBJECT>
#     [SOURCES <file>...]
#     [MODULES <target>...]      # OBJECT library modules to compose into this lib
#     [DEPENDENCIES <target>...]
#     [INCLUDE_DIRS <dir>...]
# )
#
# Creates a library with project-standard settings:
#   - Links simplish_compiler_options
#   - Adds platform compile definitions
#   - Sets include directories (defaults to current source dir)
#   - Enables LTO for Release/Shipping builds
#   - Can compose OBJECT library modules via the MODULES parameter
# ---------------------------------------------------------------------------
function(simplish_add_library name)
    cmake_parse_arguments(ARG "" "TYPE" "SOURCES;MODULES;DEPENDENCIES;INCLUDE_DIRS" ${ARGN})

    if(NOT ARG_TYPE)
        set(ARG_TYPE STATIC)
    endif()

    # Build the source list from OBJECT modules + explicit sources.
    # Skip INTERFACE modules (header-only / no-source modules created by
    # simplish_add_module when no .cpp files exist yet) because
    # $<TARGET_OBJECTS:...> is invalid for INTERFACE libraries.
    set(_all_sources ${ARG_SOURCES})
    if(ARG_MODULES)
        foreach(_mod ${ARG_MODULES})
            get_target_property(_mod_type ${_mod} TYPE)
            if(_mod_type STREQUAL "OBJECT_LIBRARY")
                list(APPEND _all_sources $<TARGET_OBJECTS:${_mod}>)
            endif()
        endforeach()
    endif()

    if(ARG_TYPE STREQUAL "INTERFACE")
        add_library(${name} INTERFACE)
        target_link_libraries(${name} INTERFACE simplish_compiler_options)
        target_compile_definitions(${name} INTERFACE ${ENGINE_PLATFORM_DEFINES})
    else()
        add_library(${name} ${ARG_TYPE} ${_all_sources})
        target_link_libraries(${name} PUBLIC simplish_compiler_options)
        target_compile_definitions(${name} PUBLIC ${ENGINE_PLATFORM_DEFINES})

        # Include directories
        if(ARG_INCLUDE_DIRS)
            target_include_directories(${name} PUBLIC ${ARG_INCLUDE_DIRS})
        else()
            target_include_directories(${name} PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            )
        endif()

        # Propagate module include directories to library consumers.
        # We link each OBJECT module as PUBLIC so CMake transitively propagates
        # INTERFACE_INCLUDE_DIRECTORIES (which contain generator expressions like
        # $<BUILD_INTERFACE:...> that only resolve at generate time, NOT configure time).
        # Using get_target_property() here would return raw generator expression strings
        # that break when re-wrapped in another target_include_directories() call.
        if(ARG_MODULES)
            foreach(_mod ${ARG_MODULES})
                target_link_libraries(${name} PUBLIC ${_mod})
            endforeach()
        endif()

        # LTO for Release/Shipping
        if(ENGINE_IPO_SUPPORTED)
            set_target_properties(${name} PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            )
        endif()
    endif()

    # Link explicit dependencies
    if(ARG_DEPENDENCIES)
        if(ARG_TYPE STREQUAL "INTERFACE")
            target_link_libraries(${name} INTERFACE ${ARG_DEPENDENCIES})
        else()
            target_link_libraries(${name} PUBLIC ${ARG_DEPENDENCIES})
        endif()
    endif()

    # NOTE: Module dependencies are now propagated transitively via the
    # target_link_libraries(${name} PUBLIC ${_mod}) calls above.
    # No need for a separate get_target_property(INTERFACE_LINK_LIBRARIES) loop,
    # which also suffered from the same configure-time vs generate-time issue.

    # Create namespaced alias
    add_library(simplish::${name} ALIAS ${name})
endfunction()

# ---------------------------------------------------------------------------
# simplish_add_executable(<name>
#     [SOURCES <file>...]
#     [DEPENDENCIES <target>...]
#     [INCLUDE_DIRS <dir>...]
# )
#
# Creates an executable with project-standard settings.
# ---------------------------------------------------------------------------
function(simplish_add_executable name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDENCIES;INCLUDE_DIRS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE simplish_compiler_options)
    target_compile_definitions(${name} PRIVATE ${ENGINE_PLATFORM_DEFINES})

    if(ARG_INCLUDE_DIRS)
        target_include_directories(${name} PRIVATE ${ARG_INCLUDE_DIRS})
    endif()

    if(ARG_DEPENDENCIES)
        target_link_libraries(${name} PRIVATE ${ARG_DEPENDENCIES})
    endif()

    # LTO for Release/Shipping
    if(ENGINE_IPO_SUPPORTED)
        set_target_properties(${name} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
        )
    endif()
endfunction()

# ---------------------------------------------------------------------------
# simplish_add_test(<name>
#     [SOURCES <file>...]
#     [DEPENDENCIES <target>...]  # Can link OBJECT modules directly
# )
#
# Creates a Catch2 test executable and registers it with CTest.
# For fast incremental builds, link OBJECT modules directly instead of the
# full static library:
#
#   simplish_add_test(core-tests
#       SOURCES test_core.cpp
#       DEPENDENCIES simplish-engine-core  # OBJECT lib, not simplish-engine
#   )
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# _simplish_collect_object_deps(<out-var> <target>...)
#
# Recursively collects every OBJECT library reachable from the given targets
# through INTERFACE_LINK_LIBRARIES, including the targets themselves.
#
# CMake contributes object files only for OBJECT libraries named directly in a
# target_link_libraries() call — transitively linked OBJECT libraries propagate
# their usage requirements (includes, defines) but not their objects. Test
# executables therefore need the full closure, or they link against headers
# whose implementations were never handed to the linker.
# ---------------------------------------------------------------------------
function(_simplish_collect_object_deps out_var)
    set(_seen "")
    set(_queue ${ARGN})
    while(_queue)
        list(POP_FRONT _queue _t)
        if(NOT TARGET ${_t})
            continue()
        endif()
        if(${_t} IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen ${_t})
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "INTERFACE_LIBRARY")
            get_target_property(_links ${_t} INTERFACE_LINK_LIBRARIES)
        else()
            get_target_property(_links ${_t} INTERFACE_LINK_LIBRARIES)
        endif()
        if(_links)
            foreach(_l ${_links})
                if(TARGET ${_l})
                    list(APPEND _queue ${_l})
                endif()
            endforeach()
        endif()
    endwhile()

    set(_objects "")
    foreach(_t ${_seen})
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "OBJECT_LIBRARY")
            list(APPEND _objects $<TARGET_OBJECTS:${_t}>)
        endif()
    endforeach()
    set(${out_var} ${_objects} PARENT_SCOPE)
endfunction()

function(simplish_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDENCIES" ${ARGN})

    # Pull in the objects of every OBJECT library reachable from DEPENDENCIES,
    # not just the directly named ones.
    _simplish_collect_object_deps(_dep_objects ${ARG_DEPENDENCIES})

    add_executable(${name} ${ARG_SOURCES} ${_dep_objects})
    target_link_libraries(${name} PRIVATE
        simplish_compiler_options
        Catch2::Catch2WithMain
    )
    target_compile_definitions(${name} PRIVATE ${ENGINE_PLATFORM_DEFINES})

    if(ARG_DEPENDENCIES)
        target_link_libraries(${name} PRIVATE ${ARG_DEPENDENCIES})
    endif()

    # Register tests with CTest via Catch2's discovery module.
    # Catch2_SOURCE_DIR is set by FetchContent_MakeAvailable(Catch2).
    # We must add the extras path here because list(APPEND CMAKE_MODULE_PATH ...)
    # inside a function only modifies a local copy — it does not persist to the
    # caller's scope. The include(Catch) call below runs within this function
    # scope where the modified path is visible, so this works correctly.
    # However, we guard against repeated include() overhead with an include guard.
    if(NOT COMMAND catch_discover_tests)
        if(DEFINED Catch2_SOURCE_DIR)
            list(APPEND CMAKE_MODULE_PATH "${Catch2_SOURCE_DIR}/extras")
        endif()
        include(Catch)
    endif()
    # Label every test with its package path relative to src/ (e.g.
    # "editor/project", "engine/gui"). Catch2 registers tests by TEST_CASE
    # title, which says nothing about where the code lives, so without this
    # there is no way to run "all the editor project tests" — see
    # scripts/test.sh, which matches filters against these labels.
    file(RELATIVE_PATH _test_package "${CMAKE_SOURCE_DIR}/src"
         "${CMAKE_CURRENT_SOURCE_DIR}")
    if(_test_package MATCHES "^\\.\\.")
        # Outside src/ — fall back to the target name so the label is never
        # a path escaping the source root.
        set(_test_package "${name}")
    endif()

    catch_discover_tests(${name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        PROPERTIES LABELS "${_test_package}"
    )
endfunction()

# ---------------------------------------------------------------------------
# simplish_target_platform_sources(<target>
#     [ALL <file>...]
#     [DESKTOP <file>...]
#     [WINDOWS <file>...]
#     [MACOS <file>...]
#     [LINUX <file>...]
#     [PS5 <file>...]
#     [XBOX <file>...]
#     [CONSOLE <file>...]
# )
#
# Adds platform-conditional source files to an existing target.
# ---------------------------------------------------------------------------
function(simplish_target_platform_sources target)
    cmake_parse_arguments(ARG "" ""
        "ALL;DESKTOP;WINDOWS;MACOS;LINUX;PS5;XBOX;CONSOLE" ${ARGN})

    if(ARG_ALL)
        target_sources(${target} PRIVATE ${ARG_ALL})
    endif()
    if(ARG_DESKTOP AND ENGINE_PLATFORM_DESKTOP)
        target_sources(${target} PRIVATE ${ARG_DESKTOP})
    endif()
    if(ARG_WINDOWS AND ENGINE_PLATFORM_WINDOWS)
        target_sources(${target} PRIVATE ${ARG_WINDOWS})
    endif()
    if(ARG_MACOS AND ENGINE_PLATFORM_MACOS)
        target_sources(${target} PRIVATE ${ARG_MACOS})
    endif()
    if(ARG_LINUX AND ENGINE_PLATFORM_LINUX)
        target_sources(${target} PRIVATE ${ARG_LINUX})
    endif()
    if(ARG_PS5 AND ENGINE_PLATFORM_PS5)
        target_sources(${target} PRIVATE ${ARG_PS5})
    endif()
    if(ARG_XBOX AND ENGINE_PLATFORM_XBOX)
        target_sources(${target} PRIVATE ${ARG_XBOX})
    endif()
    if(ARG_CONSOLE AND ENGINE_PLATFORM_CONSOLE)
        target_sources(${target} PRIVATE ${ARG_CONSOLE})
    endif()
endfunction()
