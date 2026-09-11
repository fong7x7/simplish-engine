# simplish-game-world — the game's SimulationSystems: every pool and phase
#
# Public headers:  include/game/world/
# Implementation:  src/
#
# The composition root of the simulation. Whatever runs the game — the
# editor's playtest, the client, a CI test — builds a GameWorld from a
# GameSetup and steps it with a sim::Simulation.

simplish_add_module(simplish-game-world
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/game-world.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/stand-in-input.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/world-nav-grid.cpp
    DEPENDENCIES simplish-engine-input
                 simplish-game-actors
                 simplish-game-combat
                 simplish-game-player
                 simplish-game-content
                 simplish-engine-physics
                 simplish-engine-sim
                 simplish-engine-spatial
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-world-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_determinism.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_game_world.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_horde_budget.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_stand_in_input.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_world_actors.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_world_combat.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/support/horde-scenario.cpp
        DEPENDENCIES simplish-game-world
    )
endif()
