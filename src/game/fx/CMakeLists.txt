# simplish-game-fx — what a fight looks like: the effect each combat cue
# plays
#
# Public headers:  include/game/fx/
# Implementation:  src/
#
# Presentation, above the simulation and never read by it: whoever steps a
# GameWorld reads the combat cues each tick left and plays them into an
# engine FxWorld — a muzzle flash for a shot fired, sparks for a shot a wall
# stops, a spray for a hit, a fireball for a blast. docs/engine/fx.md.

simplish_add_module(simplish-game-fx
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-fx.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-render-fx
                 simplish-game-combat
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-fx-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_combat_fx.cpp
        DEPENDENCIES simplish-game-fx
    )
endif()
