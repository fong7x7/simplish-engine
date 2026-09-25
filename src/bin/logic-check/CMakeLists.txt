# simplish-logic-check — runs a freshly built game logic library in a
# process of its own, before the editor loads it (ADR-011)
#
# Built with the editor, which runs it as the last step of Build Game
# Logic: a crash or a hang there fails the build instead of taking the
# editor down.

simplish_add_executable(simplish-logic-check-app
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    DEPENDENCIES simplish-editor
)

set_target_properties(simplish-logic-check-app PROPERTIES
    OUTPUT_NAME simplish-logic-check)

# Where the editor finds it (editor-toolchain.cpp), and a guarantee that
# building the editor builds it.
target_compile_definitions(simplish-editor-build PRIVATE
    SIMPLISH_LOGIC_CHECK="$<TARGET_FILE:simplish-logic-check-app>")
add_dependencies(simplish-editor-app simplish-logic-check-app)

# Tests that run the checker build it first.
get_property(_needs_check GLOBAL PROPERTY SIMPLISH_NEEDS_LOGIC_CHECK)
foreach(_target IN LISTS _needs_check)
    add_dependencies(${_target} simplish-logic-check-app)
endforeach()
