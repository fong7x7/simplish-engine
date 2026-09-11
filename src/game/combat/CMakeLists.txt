# simplish-game-combat — what attacks do: projectiles, hazard pools, blasts,
# and the effects buffer that carries hits to the damage phase
#
# Public headers:  include/game/combat/
# Implementation:  src/
#
# Below the actors and the world: an actor appends what its attack does to
# a CombatEffects, the world spawns shots and hazards from it, steps them,
# and applies the hits in its damage phase. Nothing here sees a player or
# actor pool — the world lists who can be hurt as CombatBody records —
# so weapons can fire through the same path when they exist.

simplish_add_module(simplish-game-combat
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-pools.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/combat-spawn.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/hazard-system.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/projectile-system.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-physics
                 simplish-engine-sim
                 simplish-engine-spatial
                 simplish-game-content
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-combat-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_combat_system.cpp
        DEPENDENCIES simplish-game-combat
    )
endif()
