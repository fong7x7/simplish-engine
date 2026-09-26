# simplish-editor-deploy — a project's deployed game, run from what a deploy
# baked (ADR-011)
#
# The library `simplish-game` is a thin main() over: read the manifest, the
# baked setup and the data tables the editor put beside the game, build the
# world with the project's logic, and run it headless. Here rather than in
# game/ because it reads the project's tables with the editor's readers —
# until ADR-007's generator bakes content into C++, they are the only ones.
#
# It is also every end of a co-op session a deployed game can be (ADR-013):
# a dedicated server, a host that plays, or a client joining one — over
# platform/net's UDP transport, with the protocol from engine/net.

simplish_add_module(simplish-editor-deploy
    INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES     ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-build-id.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-client.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-content.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-game.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-level.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-net-world.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-pacer.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-replay.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-server.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/deployed-session.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/desync-report.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/desync-text.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/seats-text.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/host-logic-test.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/logic-check.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/src/logic-test-runner.cpp
    DEPENDENCIES simplish-editor-build
                 simplish-editor-shell
                 simplish-game-logic
                 simplish-game-sdk
                 simplish-game-world
                 simplish-engine-net
                 simplish-platform-net
)

# The engine revision, baked in on every build: what a co-op client's
# build id is made of, so a server refuses a client built from other code
# (deployed-build-id.h). Only rewritten when the revision changes.
set(_revision_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(simplish-revision
    COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUT=${_revision_dir}/simplish-revision.h
            -P ${CMAKE_SOURCE_DIR}/cmake/SimplishRevision.cmake
    BYPRODUCTS ${_revision_dir}/simplish-revision.h
    COMMENT "Checking the engine revision")
add_dependencies(simplish-editor-deploy simplish-revision)
target_include_directories(simplish-editor-deploy PRIVATE ${_revision_dir})

file(GLOB _deploy_tests "${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp")
if(ENGINE_BUILD_TESTS AND _deploy_tests)
    simplish_add_test(simplish-editor-deploy-tests
        SOURCES      ${_deploy_tests}
                     ${CMAKE_CURRENT_SOURCE_DIR}/test/support/deployed-content-fixture.cpp
        DEPENDENCIES simplish-editor-deploy
    )
    target_include_directories(simplish-editor-deploy-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()
