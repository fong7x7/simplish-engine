# simplish-editor-agent — the editor's tool surface for agents
#
# A pure function of EditorShellState and one JSON call, plus the small
# value it hands back for work only the running editor can do. Nothing here
# opens a socket or touches a widget, so the whole API is testable with no
# window and no GPU; the transport lives in src/platform/agent/ and the
# binding to the running editor in editor-agent-service.cpp.

set(_agent_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/src/agent-call.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/agent-commands.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/agent-dispatch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/agent-json-values.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/agent-state-json.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/editor-agent-service.cpp"
)

simplish_add_module(simplish-editor-agent
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${_agent_sources}
    DEPENDENCIES
        simplish-editor-shell
        simplish-engine-core
        simplish-platform-agent
        nlohmann_json::nlohmann_json
)

file(GLOB _agent_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _agent_tests)
    simplish_add_test(simplish-editor-agent-tests
        SOURCES      ${_agent_tests}
        DEPENDENCIES simplish-editor-agent
    )
endif()
