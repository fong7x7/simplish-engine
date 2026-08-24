# simplish-engine-platform-steam — OBJECT library module
#
# Steamworks SDK integration: Cloud saves, Steam Input, achievements,
# Workshop, Steam Networking Sockets. Desktop only, soft dependency.

simplish_add_module(simplish-engine-platform-steam
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core
)
