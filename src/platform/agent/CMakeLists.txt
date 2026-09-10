# simplish-platform-agent — the loopback transport for the editor's agent API
#
# Sockets are platform code, so the server lives here rather than beside the
# tool surface in src/editor/agent/. It knows nothing about tools: it reads
# HTTP requests off 127.0.0.1 and hands each to a callback. Desktop only —
# nothing on a console opens a port for an agent.

simplish_add_module(simplish-platform-agent
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core
)
# Winsock lives in its own import library; POSIX sockets are in libc.
if(ENGINE_PLATFORM_WINDOWS)
    target_link_libraries(simplish-platform-agent PUBLIC ws2_32)
endif()

file(GLOB _agent_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _agent_tests)
    simplish_add_test(simplish-platform-agent-tests
        SOURCES      ${_agent_tests}
        DEPENDENCIES simplish-platform-agent
    )
endif()
