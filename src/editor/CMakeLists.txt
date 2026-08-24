# simplish-editor — the desktop editor library
#
# Links platform (for DesktopGameClient and the RHI) which links the engine.
# Desktop only; never built for consoles.
#
#   bin/editor ──► editor/ ──► platform/ ──► engine/

add_subdirectory(project)
add_subdirectory(shell)

simplish_add_library(simplish-editor
    TYPE    STATIC
    MODULES simplish-editor-project
            simplish-editor-shell
)

target_link_libraries(simplish-editor PUBLIC simplish-platform)
