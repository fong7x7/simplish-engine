# simplish-editor-deploy — a project's deployed game, run from what a deploy
# baked (ADR-011)
#
# The library `simplish-game` is a thin main() over: read the manifest, the
# baked setup and the data tables the editor put beside the game, build the
# world with the project's logic, and run it headless. Here rather than in
# game/ because it reads the project's tables with the editor's readers —
# until ADR-007's generator bakes content into C++, they are the only ones.

simplish_add_module(simplish-editor-deploy
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-game.cpp
    DEPENDENCIES simplish-editor-build
                 simplish-editor-shell
                 simplish-game-logic
                 simplish-game-world
)

file(GLOB _deploy_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _deploy_tests)
    simplish_add_test(simplish-editor-deploy-tests
        SOURCES      ${_deploy_tests}
        DEPENDENCIES simplish-editor-deploy
    )
endif()
