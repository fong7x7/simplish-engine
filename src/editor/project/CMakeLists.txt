# simplish-editor-project — project discovery, loading, and the recent list
#
# Owns the on-disk project format (.simplish/project.json). Depends on
# nothing above the engine core, so project loading is unit-testable with no
# window, no GPU, and no editor shell.

simplish_add_module(simplish-editor-project
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SRC_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDENCIES simplish-engine-core nlohmann_json::nlohmann_json
)

# Stamped into new projects' manifests so a project records the engine that
# created it.
target_compile_definitions(simplish-editor-project
    PRIVATE SIMPLISH_ENGINE_VERSION="${PROJECT_VERSION}")

file(GLOB _project_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _project_tests)
    simplish_add_test(simplish-editor-project-tests
        SOURCES      ${_project_tests}
        DEPENDENCIES simplish-editor-project
    )
endif()
