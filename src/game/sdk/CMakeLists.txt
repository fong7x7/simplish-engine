# simplish-game-sdk — the SDK a project's game logic is written with
# (ADR-011, docs/game/sdk.md)
#
# Public headers:  include/game/sdk/
# Implementation:  src/
#
# On top of game/logic's interface — what the engine lets a project's logic
# read and change — the tools a logic keeps needing: a Game base with event
# hooks, queries over players, actors and events, per-entity data, the
# logic's own events and schedule, timers and phases, spawn patterns and
# dice. Everything here calls the world through
# GameLogicWorld and nothing else, so its sources compile into a project's
# own module — cmake/SimplishGameLogic.cmake does that — as well as into the
# engine. It may depend on nothing a module would have to link.

simplish_add_module(simplish-game-sdk
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-filter.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/actor-queries.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/event-filter.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/event-queries.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/game.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/logic-tests.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/player-input.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/player-queries.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/random.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ring-spawn.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/weapon.cpp
    DEPENDENCIES simplish-game-logic
                 simplish-engine-input
                 simplish-engine-math
)

if(ENGINE_BUILD_TESTS)
    # Tested against the real world, which the SDK itself never links.
    simplish_add_test(simplish-game-sdk-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_actor_queries.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_entity_data.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_event_queries.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_events.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_game.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ring_spawn.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_schedule.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_timing.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_weapon.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/support/sdk-rig.cpp
        DEPENDENCIES simplish-game-sdk
                     simplish-game-world
    )
endif()
