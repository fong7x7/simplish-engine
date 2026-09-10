# simplish-engine-input — held actions, and the PlayerInput a tick runs on
#
# Public headers:  include/engine/input/
# Implementation:  src/
#
# Platform-free: whatever reads keys or pads — the desktop client, the
# editor's playtest — binds them to InputActions and asks this for the
# quantised input. Rebinding, gamepads and action maps arrive here too.

simplish_add_module(simplish-engine-input
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/held-actions.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/player-input-builder.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-sim
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-input-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_held_actions.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_player_input_builder.cpp
        DEPENDENCIES simplish-engine-input
    )
endif()
