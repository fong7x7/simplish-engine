# simplish-engine-math — vector, matrix, and scalar math
#
# Public headers:  include/engine/math/
# Implementation:  src/
#
# Simulation code calls into this package every tick, so what it does there
# is under the determinism contract (ADR-002): no libm transcendentals —
# sin-cos.h is the deterministic replacement for sin and cos.

simplish_add_module(simplish-engine-math
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${CMAKE_CURRENT_SOURCE_DIR}/src/mat4.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/math.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/quat.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/sin-cos.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/turn-toward.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/vec2.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/vec3.cpp
)

if(ENGINE_BUILD_TESTS)
    simplish_add_test(simplish-engine-math-tests
        SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_sin_cos.cpp
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/test_turn_toward.cpp
        DEPENDENCIES simplish-engine-math
    )
endif()
