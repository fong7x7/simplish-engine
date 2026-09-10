# Simplish source root
#
# All buildable code lives here, organised into packages. See
# docs/development/code-layout.md for the layout rules, and
# REQUIREMENTS.md §5 for the dependency direction between layers.

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(platform)

if(ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

add_subdirectory(bin)
