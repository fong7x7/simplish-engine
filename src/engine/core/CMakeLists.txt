# simplish-engine-core — allocators, logging, event bus, engine init, plugins
#
# Public headers:  include/engine/core/
# Implementation:  src/

simplish_add_module(simplish-engine-core
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-math
        nlohmann_json::nlohmann_json
)

# Platform dynamic library loader (dlopen/dlclose/dlsym) for the plugin host.
if(UNIX)
    target_link_libraries(simplish-engine-core PUBLIC ${CMAKE_DL_LIBS})
endif()

file(GLOB _core_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _core_tests)
    simplish_add_test(simplish-engine-core-tests
        SOURCES      ${_core_tests}
        DEPENDENCIES simplish-engine-core
    )
endif()
