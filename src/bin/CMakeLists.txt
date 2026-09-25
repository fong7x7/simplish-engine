# Executables. Each is a thin entry point over a library package.

if(ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
    # The deployed game reads a project's tables with the editor's readers
    # until content is baked to C++ (ADR-007), so it is built beside it.
    add_subdirectory(game)
    add_subdirectory(logic-check)
endif()
