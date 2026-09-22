# simplish-engine-render-sprite — billboarded sprites cut out of a sheet
#
# Public headers:  include/engine/render-sprite/
# Implementation:  src/
#
# The first slice of ADR-003's sprite half: how a sheet image is cut into
# frames, which frame a clock is showing, and the quad one frame is drawn
# on. Geometry and arithmetic only — the quad goes through the mesh
# pipeline, whose alpha-test cutout (`MESH_ALPHA_CUTOFF`) is what gives a
# sprite hard edges that depth-write correctly. Presentation only (ADR-002):
# nothing here runs in a tick.

simplish_add_module(simplish-engine-render-sprite
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/sprite-quad.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/sprite-sheet-frames.cpp
    DEPENDENCIES simplish-engine-math
                 simplish-engine-render
                 simplish-engine-render-mesh
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-render-sprite-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_sprite_quad.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_sprite_sheet_frames.cpp
        DEPENDENCIES simplish-engine-render-sprite
    )
endif()
