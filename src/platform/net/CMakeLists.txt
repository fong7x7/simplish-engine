# simplish-platform-net — the co-op session's sockets: ENet over UDP behind
# the engine's NetTransport (ADR-013)
#
# Public headers:  include/engine/net/ (listenUdp, connectUdp)
# Implementation:  src/
#
# Desktop only. The protocol and both ends of the session are engine code
# (src/engine/net/); this is the one piece that touches a socket. A
# console's session goes through its distributor's relay instead, which is
# another NetTransport (Platform §4.3, PLT-DST-5).

simplish_add_module(simplish-platform-net
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/enet-transport.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/udp-lan.cpp
    DEPENDENCIES simplish-engine-net
)
# PUBLIC so whatever links these objects links ENet with them.
target_link_libraries(simplish-platform-net PUBLIC enet::enet)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-platform-net-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_udp_lan.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_udp_transport.cpp
        DEPENDENCIES simplish-platform-net
    )
endif()
