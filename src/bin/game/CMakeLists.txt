# simplish-game — a project's deployed game (ADR-011)
#
# Configured with -DSIMPLISH_PROJECT_DIR=<project>, as Build ▸ Deploy Game
# does, the project's own `src/` is added here and its game logic linked
# straight into the executable: the same sources, flags and entry points a
# playtest loaded as a library. Without it, the game builds with no logic.
#
# Headless today: runDeployedGame is the dedicated-host and CI run of
# Engine §5. The rendered client joins it here when it is written.

set(SIMPLISH_PROJECT_DIR "" CACHE PATH
    "A project whose src/ game logic the deployed game links in")

if(SIMPLISH_PROJECT_DIR AND EXISTS "${SIMPLISH_PROJECT_DIR}/src/CMakeLists.txt")
    # Read by the project's own CMakeLists.txt to find SimplishGameLogic.cmake.
    set(SIMPLISH_ROOT "${CMAKE_SOURCE_DIR}")
    add_subdirectory("${SIMPLISH_PROJECT_DIR}/src"
                     "${CMAKE_CURRENT_BINARY_DIR}/project-logic")
    set(_game_logic_sources $<TARGET_OBJECTS:simplish-project-logic>)
    message(STATUS "simplish-game: linking the game logic of ${SIMPLISH_PROJECT_DIR}")
else()
    set(_game_logic_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/no-game-logic.cpp)
endif()

simplish_add_executable(simplish-game-app
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
                 ${_game_logic_sources}
    DEPENDENCIES simplish-editor
)

# $<1:...> keeps a multi-config generator from adding a Release/ folder:
# the editor copies the game from exactly one place (editor-build-paths.h).
set_target_properties(simplish-game-app PROPERTIES
    OUTPUT_NAME              simplish-game
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}>")
