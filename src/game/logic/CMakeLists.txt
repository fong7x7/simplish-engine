# simplish-game-logic — the API a project's own C++ game logic is written
# against, and the host's handle on one instance of it (ADR-011)
#
# Public headers:  include/game/logic/
# Implementation:  src/
#
# The headers are what a project's `src/` compiles against, standalone,
# with no engine library linked: every call from the logic into the world
# goes through an interface the host implements. So they may include only
# header-only packages — engine/math's types (its few out-of-line functions
# are compiled into the module), engine/sim's tick input and game/content's
# faction — and nothing here may grow a dependency on a compiled symbol a
# module would have to link.

simplish_add_module(simplish-game-logic
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/game-logic-instance.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-physics
                 simplish-engine-sim
                 simplish-game-content
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-logic-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_game_logic_instance.cpp
        DEPENDENCIES simplish-game-logic
    )
endif()
