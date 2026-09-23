# simplish-engine-input — held actions, and the PlayerInput a tick runs on
#
# Public headers:  include/engine/input/
# Implementation:  src/
#
# Platform-free: the vocabulary input is spoken in — actions, the canonical
# pad buttons and axes, the bindings between them and their JSON — and the
# quantised input a tick runs on. Which pads exist and how they are read is
# the platform's (src/platform/input/); nothing here knows a device.

simplish_add_module(simplish-engine-input
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/action-values.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-actions.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-button-label.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-rumble.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-seats.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-set.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/gamepad-state.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/held-actions.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/input-bindings.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/input-bindings-json.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/player-input-builder.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-sim
                 nlohmann_json::nlohmann_json
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-input-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_action_values.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_actions.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_button_label.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_rumble.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_seats.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_set.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepad_state.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_held_actions.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_input_bindings.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_input_bindings_json.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_player_input_builder.cpp
        DEPENDENCIES simplish-engine-input
    )
endif()
