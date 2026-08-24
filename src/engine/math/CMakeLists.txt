# simplish-engine-math — vector, matrix, and scalar math
#
# Public headers:  include/engine/math/
# Implementation:  src/

simplish_add_module(simplish-engine-math
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
)

file(GLOB _math_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _math_tests)
    simplish_add_test(simplish-engine-math-tests
        SOURCES      ${_math_tests}
        DEPENDENCIES simplish-engine-math
    )
endif()
