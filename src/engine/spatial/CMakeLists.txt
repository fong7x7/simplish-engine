# simplish-engine-spatial — where characters can go, and how to get there
#
# Public headers:  include/engine/spatial/
# Implementation:  src/
#
# Engine REQUIREMENTS §6's spatial structures: a navigation grid built from
# the level's solid boxes, line of sight across it, A* over it, flow fields
# a crowd shares, and a neighbour grid for who is near whom. Everything
# here runs inside the tick, so it is under the determinism contract
# (ADR-002): integer costs, fixed tie-breaks, no allocation per query.
#
# A red-lane path (compound-engineering.md): it decides where every actor in
# every session walks.

simplish_add_module(simplish-engine-spatial
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/flow-field.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/flow-field-builder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/grid-step.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/line-of-sight.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/nav-grid.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/nav-grid-fit.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/nearest-open-cell.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/neighbor-grid.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/path-finder.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/path-smoothing.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/reachability.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-physics
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-spatial-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_flow_field.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_line_of_sight.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_nav_grid.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_nav_grid_fit.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_nearest_open_cell.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_neighbor_grid.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_path_finder.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_path_smoothing.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_reachability.cpp
        DEPENDENCIES simplish-engine-spatial
                     simplish-engine-core
    )
endif()
