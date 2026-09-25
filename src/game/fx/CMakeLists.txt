# simplish-game-fx — what a fight looks like: the effect each combat cue
# plays
#
# Public headers:  include/game/fx/
# Implementation:  src/
#
# Presentation, above the simulation and never read by it: whoever steps a
# GameWorld reads the combat cues each tick left and plays them into an
# engine FxWorld — a muzzle flash for a shot fired, sparks for a shot a wall
# stops, a spray for a hit, a fireball for a blast (docs/engine/fx.md) — and
# into a pad's motors (combat-rumble.h) — and into the sound each one makes
# (combat-sounds.h). Footsteps are here too: when somebody walking takes a
# step, what they are standing on, and what that sounds like for their feet
# (footstep-*.h).

simplish_add_module(simplish-game-fx
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-fx.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-rumble.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-sounds.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/footstep-sounds.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/footstep-surfaces.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/footstep-tracker.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/named-fx-effect.cpp
    DEPENDENCIES simplish-engine-audio
                 simplish-engine-input
                 simplish-engine-math
                 simplish-engine-render-fx
                 simplish-engine-render-ground
                 simplish-game-combat
                 simplish-game-content
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-fx-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_combat_fx.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_combat_rumble.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_combat_sounds.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_footstep_sounds.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_footstep_surfaces.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_footstep_tracker.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_named_fx_effect.cpp
        DEPENDENCIES simplish-game-fx
    )
endif()
