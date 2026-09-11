# simplish-game-actors — the enemies and NPCs: their pool, and what a tick
# does to them (Game REQUIREMENTS §5, ADR-009)
#
# Public headers:  include/game/actors/
# Implementation:  src/
#
# An actor perceives the players, runs its behavior's state machine, plans
# a path across the level's navigation grid (engine/spatial), and moves and
# turns on the deterministic tick. Everything here is simulation code under
# the determinism contract (ADR-002).

simplish_add_module(simplish-game-actors
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-brain.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-conditions.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-decision.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-intents.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-movement.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-perception.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-planning.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-pool.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-queries.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-steering.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-system.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-workspace.cpp
    DEPENDENCIES simplish-engine-core
                 simplish-engine-input
                 simplish-engine-math
                 simplish-engine-physics
                 simplish-engine-sim
                 simplish-engine-spatial
                 simplish-game-content
                 simplish-game-player
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-actors-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/support/actor-arena.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_brain.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_movement.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_patrol.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_perception.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_states.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_system.cpp
        DEPENDENCIES simplish-game-actors
    )
    target_include_directories(simplish-game-actors-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
