# simplish-editor-shell — editor chrome: title bar, menu bar, toolbar, viewport
#
# SimplishEditor extends the platform DesktopGameClient. The widgets are
# engine GuiWidget subclasses, so they are constructible and testable without
# a window or a GPU.

set(_shell_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-asset-panel-widget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-asset-scan.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-menu-bar-widget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-placement-transform.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-toolbar-widget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-viewport-widget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/iso-view-matrix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/simplish-editor.cpp"
)

simplish_add_module(simplish-editor-shell
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${_shell_sources}
    DEPENDENCIES
        simplish-editor-project
        simplish-engine-core
        simplish-engine-gui
        simplish-engine-render
        simplish-engine-render-mesh
        simplish-platform-client
)

file(GLOB _shell_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _shell_tests)
    simplish_add_test(simplish-editor-shell-tests
        SOURCES      ${_shell_tests}
        DEPENDENCIES simplish-editor-shell
    )
endif()
