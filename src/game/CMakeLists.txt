# simplish-game — all gameplay: characters, players, actors (enemies and
# NPCs), what their attacks do and what that looks like now; weapons, the
# director and run rules as they are written
# (Game REQUIREMENTS)
#
# Platform-agnostic, and links only the engine:
#
#   game/ ──► engine/
#
#   content ──► player ──┐
#        └────► combat ──┴─► actors ──► world
#                  └──► fx  (presentation: what each combat cue looks like)

add_subdirectory(content)
add_subdirectory(player)
add_subdirectory(combat)
add_subdirectory(actors)
add_subdirectory(world)
add_subdirectory(fx)

simplish_add_library(simplish-game
    TYPE    STATIC
    MODULES simplish-game-content
            simplish-game-player
            simplish-game-combat
            simplish-game-actors
            simplish-game-world
            simplish-game-fx
)

target_link_libraries(simplish-game PUBLIC simplish-engine)
