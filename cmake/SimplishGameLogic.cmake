# SimplishGameLogic.cmake — build a project's own C++ game logic (ADR-011)
#
# Included by a project's `src/CMakeLists.txt`, which calls
#
#   simplish_game_logic(SOURCES a.cpp b.cpp ...)
#
# once. The same file builds the logic two ways, and which one depends on
# who included it:
#
#   Standalone — the editor's Build ▸ Build Game Logic runs CMake on the
#   project's `src/` with -DSIMPLISH_ROOT=<engine>. The logic becomes a
#   shared library, `game-logic.dylib` / `.so` / `.dll`, written to the top
#   of the build tree, which every playtest loads a fresh copy of. It is
#   compiled with the engine's own flags (SimplishCompilerOptions), links
#   no engine library, and carries its own copy of engine/math: everything
#   else it calls is an interface the editor implements.
#
#   In the engine — Build ▸ Deploy Game configures the engine itself with
#   -DSIMPLISH_PROJECT_DIR=<project>, and src/bin/game adds the project's
#   `src/` as a subdirectory. The logic becomes an OBJECT library,
#   `simplish-project-logic`, linked straight into the deployed game.
#
# Either way the sources, the flags and the exported entry points are the
# same, so what a playtest ran is what the deployed game runs.

if(NOT DEFINED SIMPLISH_ROOT OR SIMPLISH_ROOT STREQUAL "")
    message(FATAL_ERROR
        "SIMPLISH_ROOT is not set. Pass the engine's source tree: "
        "cmake -S src -B build/logic -DSIMPLISH_ROOT=/path/to/simplish")
endif()

# The public include roots a project's logic compiles against. game/logic's
# headers name only these, and every one of them is header-only apart from
# engine/math, whose sources are compiled into the module.
set(SIMPLISH_GAME_LOGIC_INCLUDE_DIRS
    "${SIMPLISH_ROOT}/src/game/logic/include"
    "${SIMPLISH_ROOT}/src/game/content/include"
    "${SIMPLISH_ROOT}/src/engine/sim/include"
    "${SIMPLISH_ROOT}/src/engine/math/include"
)

# The file name the editor looks for (editor-build-paths.h).
set(SIMPLISH_GAME_LOGIC_NAME "game-logic")

function(_simplish_game_logic_in_engine sources)
    add_library(simplish-project-logic OBJECT ${sources})
    target_include_directories(simplish-project-logic PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(simplish-project-logic PUBLIC simplish-game-logic)
endfunction()

function(_simplish_game_logic_standalone sources)
    file(GLOB _math_sources "${SIMPLISH_ROOT}/src/engine/math/src/*.cpp")
    add_library(simplish-project-logic MODULE ${sources} ${_math_sources})
    target_link_libraries(simplish-project-logic PRIVATE
        simplish_compiler_options)
    target_compile_definitions(simplish-project-logic PRIVATE
        ${ENGINE_PLATFORM_DEFINES})
    target_include_directories(simplish-project-logic PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR} ${SIMPLISH_GAME_LOGIC_INCLUDE_DIRS})
    # $<1:...> keeps multi-config generators from adding a Debug/ folder:
    # the editor looks in exactly one place.
    set_target_properties(simplish-project-logic PROPERTIES
        OUTPUT_NAME              ${SIMPLISH_GAME_LOGIC_NAME}
        PREFIX                   ""
        CXX_VISIBILITY_PRESET    hidden
        VISIBILITY_INLINES_HIDDEN ON
        LIBRARY_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}>"
        RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}>")
    if(APPLE)
        set_target_properties(simplish-project-logic PROPERTIES SUFFIX ".dylib")
    endif()
endfunction()

# simplish_game_logic(SOURCES <file>...)
#
# The project's game logic, from SOURCES — relative to the calling
# CMakeLists.txt, and listed explicitly: nothing is globbed.
function(simplish_game_logic)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "simplish_game_logic: list the logic's SOURCES")
    endif()
    if(TARGET simplish-game-logic)
        _simplish_game_logic_in_engine("${ARG_SOURCES}")
    else()
        _simplish_game_logic_standalone("${ARG_SOURCES}")
    endif()
endfunction()

# Standalone, the engine's platform and compiler settings have not been
# read yet: read them, so the logic is compiled exactly as the engine is —
# the determinism flags above all (ADR-002).
if(NOT TARGET simplish-game-logic AND NOT TARGET simplish_compiler_options)
    list(APPEND CMAKE_MODULE_PATH "${SIMPLISH_ROOT}/cmake")
    include(SimplishPlatform)
    include(SimplishCompilerOptions)
endif()
