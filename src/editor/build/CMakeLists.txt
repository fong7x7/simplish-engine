# simplish-editor-build — building a project's own C++ game logic, and
# deploying its game (ADR-011)
#
# The pieces the editor's Build menu is made of, each testable with no
# window: the paths a project's builds write to, the scaffold a project's
# `src/` starts from, the CMake command lines, a job running them on a
# thread, loading the built library, and the setups and manifest a
# deployed game starts from. `SimplishEditor` owns the job and the library;
# this package knows nothing of it.

simplish_add_module(simplish-editor-build
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-build-commands.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-build-diagnostics.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-build-job.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-build-log.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-build-paths.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-deploy-manifest.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-logic-library.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-logic-source.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-logic-templates.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-project-guide.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-setup-json.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/editor-toolchain.cpp
    DEPENDENCIES simplish-editor-project
                 simplish-engine-core
                 simplish-game-content
                 simplish-game-logic
                 simplish-game-world
                 nlohmann_json::nlohmann_json
)

# The toolchain that built the editor is the one it builds a project's
# logic with: the logic is called through C++ vtables, which only agree
# between libraries one compiler laid out (editor-toolchain.h).
target_compile_definitions(simplish-editor-build PRIVATE
    SIMPLISH_ENGINE_ROOT="${CMAKE_SOURCE_DIR}"
    SIMPLISH_CMAKE_COMMAND="${CMAKE_COMMAND}"
    SIMPLISH_CMAKE_GENERATOR="${CMAKE_GENERATOR}"
    SIMPLISH_MAKE_PROGRAM="${CMAKE_MAKE_PROGRAM}"
    SIMPLISH_CXX_COMPILER="${CMAKE_CXX_COMPILER}"
)
# SIMPLISH_LOGIC_CHECK, the path of simplish-logic-check, is defined by
# src/bin/logic-check, which is configured after this package.

file(GLOB _build_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _build_tests)
    simplish_add_test(simplish-editor-build-tests
        SOURCES      ${_build_tests}
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/support/build-temp-dir.cpp
        DEPENDENCIES simplish-editor-build
    )
    # The end-to-end test builds a scaffolded project's logic with the real
    # toolchain and loads it; it needs to know where the engine is.
    target_compile_definitions(simplish-editor-build-tests PRIVATE
        SIMPLISH_TEST_ENGINE_ROOT="${CMAKE_SOURCE_DIR}")
    # The checked-build tests run the checker src/bin/logic-check makes.
    if(ENGINE_BUILD_EDITOR)
        set_property(GLOBAL APPEND PROPERTY SIMPLISH_NEEDS_LOGIC_CHECK
                     simplish-editor-build-tests)
    endif()
endif()
