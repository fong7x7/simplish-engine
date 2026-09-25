#include <editor/build/editor-project-guide.h>
#include <editor/project/project-text-file.h>
#include <string_view>
#include <system_error>

namespace eng::editor {

namespace {

  /// The guide, with `@ENGINE@` where the engine's path goes.
  constexpr std::string_view GUIDE = R"(# This game project

A game made with the Simplish engine and editor. Engine: `@ENGINE@`.
Its documents are the authority; this file is the map.

## Layout

| Path | What |
|---|---|
| `.simplish/project.json` | The manifest |
| `content/levels/<id>.level.json` | Levels, authored in the editor |
| `content/data/*.data.json` | Data tables: characters, behaviors, enemies, sounds — `@ENGINE@/docs/editor/project-format.md` §8 |
| `assets/` | Models, sprites, sounds |
| `src/` | The game's own rules, in C++ — see below |
| `build/` | Everything the editor builds. Never edit, never commit |

## Working through the editor

Drive the running editor through its agent tools (MCP, or HTTP on
127.0.0.1:8787 — `@ENGINE@/docs/editor/agent-api.md`). `describe` lists
every tool. The loop:

1. Author the level: `place_asset`, `set_behavior`, `add_player_start`,
   `paint_ground`; then `run_command` `save`.
2. Write or change the logic in `src/`.
3. `run_command` `build_game_logic`, then `get_build` with `"wait": true`:
   it answers when the build has finished. On failure,
   `build.diagnostics` gives file, line, column and message. Every build is also run for ten seconds of the
   open level in a process of its own, twice: a crash fails it, and so
   does a run that ends differently the second time (nondeterminism).
   Then the tests in `src/tests/` run; `build.tests` says how each went,
   and a failed one fails the build.
4. `start_playtest`, `send_input` (`fire`, `move_x`, `aim_x`, …),
   `step_playtest` — which answers with the playtest at the tick it
   reached: actors, `outcome`, `logic_log`. `stop_playtest` when done.
   Calls like these answer with what they did; no sleeping between them.
   `get_log` has what the editor warned of — level problems, failed
   builds, the logic's own lines.
5. `run_command` `deploy_game` puts a standalone build in `build/deploy/`.

## Writing game logic

`src/` is C++20 against the game SDK — `#include <game/sdk/sdk.h>`,
namespace `eng::game::sdk`. Guide: `@ENGINE@/docs/game/sdk.md`. Rules and
background: `@ENGINE@/docs/game/logic.md`. Headers:
`@ENGINE@/src/game/sdk/include/game/sdk/` and
`@ENGINE@/src/game/logic/include/game/logic/game-logic-world.h`.

- Derive from `sdk::Game`, override its hooks (`onTick`, `onActorDied`, …),
  and export it once: `SIMPLISH_GAME_LOGIC(ClassName)`.
- List every source file in `src/CMakeLists.txt`; nothing is globbed.
- The logic is simulation, run on a deterministic tick. Never read a
  clock, never use `std::rand` or `<random>`, never iterate an unordered
  container, never keep state in globals or statics. Time is
  `world.tick()` (60 a second, `sdk::seconds(n)`); dice are
  `world.random()` and `sdk::chance`/`between`/`pick`.
- Every member a later tick decides anything by goes into `onHash`.
  `sdk::EntityData`, `sdk::Phase` and `sdk::Cooldown` hash themselves.
- `LogicActor::id` and event ids are views valid for one tick; copy them
  to keep them.
- Players can walk off the level's floor; spawn round fixed places —
  where they started, a named prop — not round them.
)";

  /// The pointer to the guide.
  constexpr std::string_view POINTER =
      "See CLAUDE.md: this project's guide for agents.\n";

  /// Write @p text to @p path unless something is there. False when it
  /// could not be written.
  bool writeIfAbsent(const std::filesystem::path& path, std::string_view text) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) || writeProjectTextFile(path, text);
  }

}  // namespace

std::string projectAgentGuide(const std::filesystem::path& engine_root) {
  std::string guide(GUIDE);
  const std::string engine = engine_root.string();
  for (size_t at = guide.find("@ENGINE@"); at != std::string::npos;
       at = guide.find("@ENGINE@", at + engine.size())) {
    guide.replace(at, 8, engine);
  }
  return guide;
}

bool writeProjectAgentGuide(const std::filesystem::path& root,
                            const std::filesystem::path& engine_root) {
  return writeIfAbsent(root / PROJECT_GUIDE_FILE_NAME,
                       projectAgentGuide(engine_root)) &&
         writeIfAbsent(root / PROJECT_AGENTS_FILE_NAME, POINTER);
}

}  // namespace eng::editor
