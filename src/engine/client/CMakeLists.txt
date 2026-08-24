# simplish-engine-client — platform-agnostic game client
#
# GameClient defines the lifecycle; RenderedGameClient adds the retained GUI
# and per-frame RHI presentation. Neither includes a platform header — the
# SDL3 window, event pump, and RHI device creation live in
# src/platform/client/ (DesktopGameClient).

set(_client_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/desktop/rendered-game-client.cpp"
)

simplish_add_module(simplish-engine-client
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${_client_sources}
    DEPENDENCIES
        simplish-engine-core
        simplish-engine-gui
        simplish-engine-render
)

file(GLOB _client_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _client_tests)
    simplish_add_test(simplish-engine-client-tests
        SOURCES      ${_client_tests}
        DEPENDENCIES simplish-engine-client
    )
endif()
