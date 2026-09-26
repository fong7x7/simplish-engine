# simplish-engine-net — the co-op session: transport interface, protocol,
# and both ends of server-relayed lockstep (ADR-005, ADR-013)
#
# Public headers:  include/engine/net/
# Implementation:  src/
#
# Platform-free. The sockets are in src/platform/net/ behind NetTransport;
# LoopbackNetwork here carries a session inside one process, which is what
# every test below runs over. Nothing here reads a clock: the caller paces
# input, and frames arrive when they arrive.

simplish_add_module(simplish-engine-net
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/lockstep-client.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/lockstep-server.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/loopback-network.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/net-codec.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/net-input-delay.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/net-trace-divergence.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/trace-ring.cpp
    DEPENDENCIES simplish-engine-core
                 simplish-engine-sim
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-net-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/support/loopback-session.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_lockstep_client.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_lockstep_server.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_loopback_network.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_net_codec.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_net_input_delay.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_net_trace_divergence.cpp
        DEPENDENCIES simplish-engine-net
    )
    target_include_directories(simplish-engine-net-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
