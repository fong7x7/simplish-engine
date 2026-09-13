# simplish-engine-render-fx — particles and flashes of light, played when
# something happens and drawn over the finished scene
#
# Public headers:  include/engine/render-fx/
# Implementation:  src/
#
# Engine REQUIREMENTS §5.3's transparent-effects pass and the first of
# §5.4's dynamic lights: bursts of particles thrown out, moved on the render
# frame's clock and drawn as camera-facing quads that the scene's depth
# hides and softens; and flashes that light meshes through the same
# `MeshLight`s placed lights do. Presentation only (ADR-002): nothing here
# runs in a tick, and its random stream is never hashed. docs/engine/fx.md
# has the details.

simplish_add_module(simplish-engine-render-fx
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-color.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-light-pool.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-particle-pool.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-quads.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-renderer.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/fx-world.cpp
    DEPENDENCIES simplish-engine-core
                 simplish-engine-math
                 simplish-engine-render
                 simplish-engine-render-mesh
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-render-fx-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_color.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_light_pool.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_particle_pool.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_quads.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_renderer.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_fx_world.cpp
        DEPENDENCIES simplish-engine-render-fx
    )
endif()
