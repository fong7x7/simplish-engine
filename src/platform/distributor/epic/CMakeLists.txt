# simplish-engine-platform-epic — OBJECT library module
#
# Epic Online Services SDK integration: Cloud saves, achievements, stats,
# leaderboards, social, matchmaking, P2P networking, commerce. Desktop only,
# soft dependency.

simplish_add_module(simplish-engine-platform-epic
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core
)
