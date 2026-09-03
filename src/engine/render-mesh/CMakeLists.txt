# simplish-engine-render-mesh — static mesh assets and their GPU submission
#
# Public headers:  include/engine/render-mesh/
# Implementation:  src/

simplish_add_module(simplish-engine-render-mesh
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES
        simplish-engine-core
        simplish-engine-math
        simplish-engine-render
)

file(GLOB _render_mesh_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _render_mesh_tests)
    simplish_add_test(simplish-engine-render-mesh-tests
        SOURCES      ${_render_mesh_tests}
        DEPENDENCIES simplish-engine-render-mesh
    )
endif()
