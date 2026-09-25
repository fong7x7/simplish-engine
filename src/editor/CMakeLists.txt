# simplish-editor — the desktop editor library
#
# Links platform (for DesktopGameClient and the RHI) which links the engine.
# Desktop only; never built for consoles.
#
#   bin/editor ──► editor/ ──► platform/ ──► engine/
#                        └──► game/ ──────────┘   (the playtest runs the game)

add_subdirectory(project)
add_subdirectory(build)
add_subdirectory(shell)
add_subdirectory(agent)
add_subdirectory(deploy)

simplish_add_library(simplish-editor
    TYPE    STATIC
    MODULES simplish-editor-project
            simplish-editor-build
            simplish-editor-shell
            simplish-editor-agent
            simplish-editor-deploy
)

target_link_libraries(simplish-editor PUBLIC simplish-platform simplish-game)
