# simplish-game-content — what the game is made of, as the simulation reads
# it: character definitions now; weapons, enemies and waves as they are
# written (Game REQUIREMENTS §3–§5, ADR-007)
#
# Public headers:  include/game/content/
# Implementation:  src/

simplish_add_module(simplish-game-content
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/character-lookup.cpp
    DEPENDENCIES simplish-engine-core
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-content-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_character_lookup.cpp
        DEPENDENCIES simplish-game-content
    )
endif()
