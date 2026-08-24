# simplish-editor-app — the editor executable
#
# Thin entry point: parse arguments, resolve paths, hand off to
# SimplishEditor. Logic that deserves a test lives in the editor library.

simplish_add_executable(simplish-editor-app
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    DEPENDENCIES simplish-editor
)

set_target_properties(simplish-editor-app PROPERTIES OUTPUT_NAME simplish-editor)
