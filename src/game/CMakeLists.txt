# simplish-game — all gameplay: characters and players now; weapons, enemies, the director
# and run rules as they are written (Game REQUIREMENTS)
#
# Platform-agnostic, and links only the engine:
#
#   game/ ──► engine/
#
#   content ──► player ──► world

add_subdirectory(content)
add_subdirectory(player)
add_subdirectory(world)

simplish_add_library(simplish-game
    TYPE    STATIC
    MODULES simplish-game-content
            simplish-game-player
            simplish-game-world
)

target_link_libraries(simplish-game PUBLIC simplish-engine)
