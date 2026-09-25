# simplish-game-ui — a game's own screens: menus and a HUD, built from the
# engine's GUI widgets (ADR-012, docs/game/ui.md)
#
# Public headers:  include/game/ui/
# Implementation:  src/
#
# A screen is data — content/ui/<id>.ui.json, read here with named problems
# — and a UiScreenView builds it into a GuiWidgetTree out of the engine's
# own panels, labels and buttons, laid out by flexbox. Presentation: the
# game logic only shows screens and sets the values they show, and a
# button pressed reaches the simulation as player input, never from here.

simplish_add_module(simplish-game-ui
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-actions.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-json-read.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-screen-json.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-screen-view.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-style-json.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-text.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ui-widget-style.cpp
    DEPENDENCIES simplish-engine-gui
                 nlohmann_json::nlohmann_json
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-game-ui-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ui_screen_json.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ui_screen_view.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ui_text.cpp
        DEPENDENCIES simplish-game-ui
    )
endif()
