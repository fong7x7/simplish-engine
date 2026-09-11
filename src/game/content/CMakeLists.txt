# simplish-game-content — what the game is made of, as the simulation reads
# it: character definitions and behaviors now; weapons, enemies and waves as
# they are written (Game REQUIREMENTS §3–§5, ADR-007, ADR-009)
#
# Public headers:  include/game/content/
# Implementation:  src/

simplish_add_module(simplish-game-content
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior-lookup.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior-names.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin-behaviors.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/character-lookup.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/enemy-lookup.cpp
    DEPENDENCIES simplish-engine-core
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-content-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_behavior_lookup.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_behavior_names.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_character_lookup.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_enemy_lookup.cpp
        DEPENDENCIES simplish-game-content
    )
endif()
