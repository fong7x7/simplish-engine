# simplish-engine-render-water — the surface of painted water
#
# Public headers:  include/engine/render-water/
# Implementation:  src/
#
# Water painted on the ground, drawn as a surface that moves: a height
# field over the water's cells stepped by the wave equation on the render
# frame's clock, pushed by whatever wades through it and by drizzle,
# packed into a texture of slopes, levels and distances to the shore, and
# shaded by the backend's water pipeline over the ground's flat water
# layer. How much of that runs is a `WaterFidelity`. Presentation only
# (ADR-002): nothing here runs in a tick. docs/engine/water.md has the
# details.

simplish_add_module(simplish-engine-render-water
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/water-corners.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/water-field.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/water-layer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/water-renderer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/water-surface-mesh.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/water-texels.cpp
    DEPENDENCIES simplish-engine-core
                 simplish-engine-math
                 simplish-engine-render
                 simplish-engine-render-mesh
                 simplish-engine-render-ground
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-render-water-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_depth.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_layer.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_fidelity.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_field.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_renderer.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_surface_mesh.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_water_texels.cpp
        DEPENDENCIES simplish-engine-render-water
    )
endif()
