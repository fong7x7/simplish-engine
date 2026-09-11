# simplish-game — all gameplay: characters, players, and actors (enemies and
# NPCs) now; weapons, the director and run rules as they are written
# (Game REQUIREMENTS)
#
# Platform-agnostic, and links only the engine:
#
#   game/ ──► engine/
#
#   content ──► player ──► actors ──► world

add_subdirectory(content)
add_subdirectory(player)
add_subdirectory(actors)
add_subdirectory(world)

simplish_add_library(simplish-game
    TYPE    STATIC
    MODULES simplish-game-content
            simplish-game-player
            simplish-game-actors
            simplish-game-world
)

target_link_libraries(simplish-game PUBLIC simplish-engine)
