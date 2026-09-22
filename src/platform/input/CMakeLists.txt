# simplish-platform-input — the pads this platform supports
#
# Public headers:  include/engine/input/
# Backends:        src/<name>/, one compiled in
#
# The engine speaks one pad vocabulary (src/engine/input/: canonical
# buttons and axes, bindings, GamepadSet); this package decides which
# physical pads exist and reads them into it. Exactly one backend is
# compiled in, so a build only carries the pad support its target has:
#
#   SDL        desktop — every pad SDL3 knows: Xbox, DualShock 4, DualSense,
#              Switch Pro, Joy-Cons, and generic pads with a mapping
#   DUALSENSE  PS5 — DualSense only; from the private console overlay
#   GAMEINPUT  Xbox — Xbox pads only; from the private console overlay
#   NONE       no pads; also where a console lands without its overlay
#
# Override with -DENGINE_GAMEPAD_BACKEND=<name>.

if(ENGINE_PLATFORM_DESKTOP)
    set(_gamepad_default SDL)
elseif(ENGINE_PLATFORM_PS5)
    set(_gamepad_default DUALSENSE)
elseif(ENGINE_PLATFORM_XBOX)
    set(_gamepad_default GAMEINPUT)
else()
    set(_gamepad_default NONE)
endif()
set(ENGINE_GAMEPAD_BACKEND "${_gamepad_default}" CACHE STRING
    "Pad backend: SDL, DUALSENSE, GAMEINPUT or NONE")

string(TOLOWER "${ENGINE_GAMEPAD_BACKEND}" _gamepad_dir)
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/${_gamepad_dir}")
    message(STATUS "Pad backend ${ENGINE_GAMEPAD_BACKEND} is not in this "
                   "tree (console backends come from the private overlay); "
                   "building with no pads")
    set(_gamepad_dir none)
endif()

set(_gamepad_sources_sdl
    "${CMAKE_CURRENT_SOURCE_DIR}/src/sdl/sdl-gamepads.cpp")
set(_gamepad_sources_none
    "${CMAKE_CURRENT_SOURCE_DIR}/src/none/no-gamepads.cpp")

simplish_add_module(simplish-platform-input
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${_gamepad_sources_${_gamepad_dir}}
    DEPENDENCIES simplish-engine-input
)
if(_gamepad_dir STREQUAL "sdl")
    target_link_libraries(simplish-platform-input PUBLIC SDL3::SDL3)
endif()

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-platform-input-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_gamepads.cpp
        DEPENDENCIES simplish-platform-input
    )
endif()
