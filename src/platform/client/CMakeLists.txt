# simplish-platform-client — SDL3 desktop client
#
# DesktopGameClient extends the engine's RenderedGameClient with an SDL3
# window, event pump, and RHI device creation via the platform factory.
# No SDL type crosses its public headers.

set(_client_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/desktop/desktop-game-client.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/desktop/desktop-platform-utility.cpp"
)

simplish_add_module(simplish-platform-client
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${_client_sources}
    DEPENDENCIES
        simplish-engine-core
        simplish-engine-gui
        simplish-engine-render
        simplish-engine-client
        simplish-platform-render
)
target_link_libraries(simplish-platform-client PUBLIC SDL3::SDL3)

file(GLOB _client_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _client_tests)
    simplish_add_test(simplish-platform-client-tests
        SOURCES      ${_client_tests}
        DEPENDENCIES simplish-platform-client
    )
endif()
