# simplish-engine-render-ground — painted ground, autotiled
#
# Public headers:  include/engine/render-ground/
# Implementation:  src/
#
# The terrain a level's floor is painted with: a grid of terrain numbers,
# the rule that shapes each quarter of a cell from its neighbours so painted
# areas join with rounded edges, the flat stacked mesh that draws it, and
# the runs a level file stores it as. Geometry and arithmetic only — the
# mesh goes through the mesh pipeline, as a sprite's quad does.

simplish_add_module(simplish-engine-render-ground
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/ground-grid.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ground-mesh.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ground-quarter-shape.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/ground-runs.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-render-mesh
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-render-ground-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ground_grid.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ground_mesh.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ground_quarter_shape.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_ground_runs.cpp
        DEPENDENCIES simplish-engine-render-ground
    )
endif()
