# Game Logic — a Project's Own Rules in C++

**Status:** first slice built — `src/game/logic/`, the logic phase in `src/game/world/`, the editor's Build menu in `src/editor/build/` and `src/editor/shell/`, and the deployed game in `src/editor/deploy/` and `src/bin/game/`.
**Decision:** [ADR-011](../decisions/ADR-011-project-game-logic-in-cpp.md).

A project's data tables say what its game is made of. Its **game logic** says the rules no table can: when a level is won, what happens at minute three, how a boss's second phase starts. It is C++, it lives in the project's own `src/` folder — not in the engine — and the editor builds it, plays it and deploys it.

---

## 1. Quick start

1. **Build ▸ New Game Logic** writes `src/CMakeLists.txt` and an example, `src/game-logic.cpp`: win by clearing the level's hostiles or by surviving 90 seconds, and heal every player a segment every ten.
2. Edit it. **Build ▸ Build Game Logic** (`Cmd`/`Ctrl`+B) compiles it in the background — about two seconds — and loads it. The status line says how it went; the compiler's errors are in `build/logic.log`.
3. **Play.** The playtest runs the logic. What it says with `world.log(...)` goes to the editor's log, prefixed `logic`.
4. **Build ▸ Deploy Game** bakes every saved level, builds the engine in Release with the logic linked in, and puts the game in `build/deploy/`. The first deploy builds the whole engine and takes minutes; later ones rebuild what changed.

A minimal logic:

```cpp
#include <game/logic/game-logic-entry.h>
#include <game/logic/game-logic.h>

namespace {

class Survive final : public eng::game::GameLogic {
public:
  void tick(eng::game::GameLogicWorld& world) override {
    if (world.tick() == 60 * 90) {
      world.endRun(eng::game::RunOutcome::WON);
    }
  }
};

}  // namespace

SIMPLISH_GAME_LOGIC(Survive)
```

`SIMPLISH_GAME_LOGIC` is written once, in one file. Every source the logic is built from is listed in `src/CMakeLists.txt`'s `simplish_game_logic(SOURCES …)`; nothing is globbed.

---

## 2. The shape

| Piece | Where | What |
|---|---|---|
| `GameLogic` | [game-logic.h](../../src/game/logic/include/game/logic/game-logic.h) | What a project subclasses: `start` (tick 0, once), `tick` (every tick), `hashState` |
| `GameLogicWorld` | [game-logic-world.h](../../src/game/logic/include/game/logic/game-logic-world.h) | The world, as the logic sees it: reads, queued writes, the random stream, the log |
| `GameLogicHash` | [game-logic-hash.h](../../src/game/logic/include/game/logic/game-logic-hash.h) | Where `hashState` folds the logic's own members |
| `LogicPlayer`, `LogicActor`, `LogicTarget` | `logic-*.h` | Plain copies of a player or an actor, and the handle a write names one by |
| `SIMPLISH_GAME_LOGIC` | [game-logic-entry.h](../../src/game/logic/include/game/logic/game-logic-entry.h) | Exports `simplishGameLogicApiVersion`, `simplishCreateGameLogic`, `simplishDestroyGameLogic` |
| `GameLogicInstance` | [game-logic-instance.h](../../src/game/logic/include/game/logic/game-logic-instance.h) | Host side: one run's instance, unmade by the module that made it |
| The logic phase | [game-world.cpp](../../src/game/world/src/game-world.cpp) `director` | Runs the logic, applies its writes |
| `simplish_game_logic` | [SimplishGameLogic.cmake](../../cmake/SimplishGameLogic.cmake) | Builds `src/` as a library (editor) or objects (deploy) |

The headers a project includes are header-only apart from engine/math, whose few sources `simplish_game_logic` compiles into the module. Everything else the logic does is a virtual call into an object the host made, so the module links against nothing of the engine's (§6).

---

## 3. A tick

The logic runs in the **director's phase** (Engine §4.1 step 7): players have moved, actors have acted, and every hit of the tick has landed; the dead have not yet been cleared away.

1. On tick 0, `start` — before the first `tick`.
2. `tick`. **Reads** see the world as the damage phase left it and never see this tick's writes, so the order the logic reads in never matters. **Writes** are queued.
3. The queue is applied in the order it was written:
   - `damage` goes through the same path as a bite or a shot: a player's grace after a hit, going down at no health; an actor's death and the blast it goes off in, whose hits land in turn.
   - `heal` gives segments back up to a full bar. A player who is down or out is not healed — reviving is a teammate's — and neither is an actor already dead this tick.
   - `endRun` has already taken effect: the first ending stands.
4. Compaction, then the tick hash.

A run is over when the logic ends it (`WON` or `LOST`), or when no player is up (`LOST`). `GameWorld::outcome` says which.

---

## 4. Rules the logic has to keep

The logic is simulation. Everything [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) holds the engine to holds it too, and nothing enforces it but the tick hash noticing afterwards.

| Do | Never |
|---|---|
| Count time in ticks: `world.tick()`, 60 a second | Read a clock — `std::chrono`, `time`, frame time |
| Draw from `world.random(bound)` | `std::rand`, `std::random_device`, a stream of your own seeded from anything but ticks |
| Iterate vectors and arrays in index order | Iterate `std::unordered_map`/`set`, or anything ordered by address |
| Keep a `LogicTarget` to name an entity across ticks | Keep an index into `actor(i)`: it names someone else after compaction |
| Fold every member a later tick decides by into `hashState`, in a fixed order | Keep state in globals or statics — they outlive the run, and a new playtest would start with them |
| Compare floats with the engine's own math, compiled with its flags (`simplish_game_logic` does this) | Turn on fast-math or FMA contraction for the logic |
| Say what happened with `world.log` | Do I/O in `tick` — files, sockets, printing |

`LogicActor::id` and `LogicActor::state` are views into the world's strings, valid until `tick` returns; copy one to keep it.

---

## 5. What the world offers

| Read | |
|---|---|
| `tick()` | The tick being simulated, from 0 |
| `input()` | Every player's quantised input this tick |
| `playerCount()`, `player(i)` | Slot, position, aim, health and full bar, up/down/out, and a `target` |
| `actorCount()`, `actor(i)` | The level's id for it (the prop it was placed as), position, facing, faction, health, the state of its behavior it is in, and a `target` |
| `outcome()` | Playing, won or lost |

| Write | |
|---|---|
| `damage(target, amount)` | Queued; applied as a hit |
| `heal(target, amount)` | Queued; up to a full bar |
| `endRun(outcome)` | The first ending stands |
| `random(bound)` | `0 … bound-1` from the logic's own stream, `LOGIC_RNG_STREAM`, so a draw added to the logic never shifts what actors roll |
| `log(message)` | Presentation: the editor's log and `get_playtest`'s `logic_log`, or the deployed game's output |

Everything else — spawning actors, reading props and the navigation grid, cueing effects and sounds — is §9's.

---

## 6. Two builds from one folder

```
<project>/
├── src/                       # the project's logic — committed
│   ├── CMakeLists.txt         # simplish_game_logic(SOURCES ...)
│   └── game-logic.cpp
└── build/                     # everything built — ignored by its own .gitignore
    ├── logic/                 # Build Game Logic's CMake tree
    │   ├── game-logic.dylib   # .so, .dll
    │   └── loaded/            # the copies the editor has loaded
    ├── logic.log
    ├── deploy-cmake/          # Deploy Game's CMake tree: the whole engine
    ├── deploy.log
    └── deploy/                # the deployed game
        ├── simplish-game
        └── game/              # manifest.json, levels/<id>.setup.json, content/data/
```

**Build Game Logic** runs `cmake -S <project>/src -B build/logic -DSIMPLISH_ROOT=<engine>` and `cmake --build`. Standalone, `SimplishGameLogic.cmake` reads the engine's own platform and compiler settings, so the module is compiled with exactly the engine's flags — determinism flags included — and with the CMake, generator and compiler the editor itself was built with ([editor-toolchain.h](../../src/editor/build/include/editor/build/editor-toolchain.h)): C++ vtables only agree between libraries one compiler laid out. `SIMPLISH_ENGINE_ROOT` in the environment points an editor moved away from its checkout at the engine tree.

The editor copies the library into `loaded/` before opening it — so the next build can overwrite it, and so the platform never hands back a cached image — checks the three exports and `GAME_LOGIC_API_VERSION`, and keeps it for the **next** playtest. A running playtest keeps the library it started with; it closes when that playtest stops. Opening a project loads the library it last built, if there is one.

**Deploy Game**:

1. Bakes every saved level — the file, not unsaved edits — into `game/levels/<id>.setup.json`: the `GameSetup` a playtest would start from, with every seat's spawn and each prop's collision box measured from its mesh. Floats are written to read back to the same bits.
2. Copies `content/data/` and writes `game/manifest.json`: the name, the levels, the one it starts on (`main`, else the first).
3. Configures the engine with `-DSIMPLISH_PROJECT_DIR=<project>` in Release; `src/bin/game` adds the project's `src/` as a subdirectory, where the same `simplish_game_logic` call makes an OBJECT library linked into `simplish-game`.
4. Copies `simplish-game` beside `game/`.

Deploy says so when the open level has unsaved edits, since it bakes the saved file. One build runs at a time. Quitting the editor while one runs does not wait for it: the build carries on to the end on its own, and nothing is loaded or copied from it.

---

## 7. The deployed game

`simplish-game` is headless today: the deterministic simulation with the project's rules linked in, every player a stand-in — the dedicated host and CI run of Engine §5, and the proof a deploy works. The rendered client that puts a person at the controls is not written ([REQUIREMENTS.md §6](../../REQUIREMENTS.md#6-repository--project-structure-target)); it will start runs from the same baked setups and link the same logic.

```bash
build/deploy/simplish-game --level main --ticks 3600 --players 2
```

It prints what the logic says, then how the run ended and the last tick's hash — the same on every platform for the same build, level, players and length. It reads the data tables with the editor's readers until content is baked to C++ ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)), which is why it lives in `src/editor/deploy/` and links the editor library.

---

## 8. Agents

| To | Call |
|---|---|
| Start a project's logic | `run_command` `new_game_logic`, or write `src/` directly |
| Build it | `run_command` `build_game_logic`, then poll `get_build` until `build.status` is `succeeded` or `failed` — `build.builds` tells your build from the last one; `build.errors` holds the compiler's complaints and `build.log` the whole transcript |
| See whether the loaded logic is current | `get_build` `logic_stale` |
| Play it | `start_playtest`, `step_playtest`; `get_playtest` reports `logic`, `outcome` and `logic_log` |
| Deploy | `run_command` `deploy_game`, then poll `get_build`; `deployed` is the folder |

---

## 9. Not yet

- **Spawning.** The actor pool is sized by the level's actors; spawning at run time is the director's job and needs the pool to grow. Until then, logic works with the actors a level places.
- **More reads** — props, the navigation grid, line of sight, projectiles — and **presentation cues** from logic (a sound, an effect).
- **Replays naming their logic.** A replay does not yet record which build of the logic it was made with, so one replayed against changed rules reports a divergence rather than refusing to start.
- **The rendered deployed client**, and assets in the deploy folder with it.
- **Stepping into logic in a debugger** works — the library is a Debug build with symbols — but nothing in the editor attaches one.
