# simplish-engine-physics — keeping characters out of solid geometry
#
# Public headers:  include/engine/physics/
# Implementation:  src/
#
# A first slice: an upright cylinder pushed out of axis-aligned boxes, and
# a broadphase that gathers the boxes near a mover. Projectile integration, swept queries and line of sight join it
# (Engine REQUIREMENTS §6). Deterministic like everything the tick calls.

simplish_add_module(simplish-engine-physics
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/box-broadphase.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/cylinder-collision.cpp
    DEPENDENCIES simplish-engine-math
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-physics-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_box_broadphase.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_cylinder_collision.cpp
        DEPENDENCIES simplish-engine-physics
                     simplish-engine-core
    )
endif()
