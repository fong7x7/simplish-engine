# simplish-game-player — the players: their pool and what a tick does to them
#
# Public headers:  include/game/player/
# Implementation:  src/

simplish_add_module(simplish-game-player
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/player-system.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-sim
                 simplish-engine-input
                 simplish-engine-physics
                 simplish-game-content
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-player-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_player_system.cpp
        DEPENDENCIES simplish-game-player
    )
endif()
