# Game Logic — a Project's Own Rules in C++

**Status:** first slice built — `src/game/logic/`, the logic phase in `src/game/world/`, the editor's Build menu in `src/editor/build/` and `src/editor/shell/`, and the deployed game in `src/editor/deploy/` and `src/bin/game/`.
**Decision:** [ADR-011](../decisions/ADR-011-project-game-logic-in-cpp.md).

A project's data tables say what its game is made of. Its **game logic** says the rules no table can: when a level is won, what happens at minute three, how a boss's second phase starts. It is C++, it lives in the project's own `src/` folder — not in the engine — and the editor builds it, plays it and deploys it.

---

## 1. Quick start

1. **Build ▸ New Game Logic** writes `src/CMakeLists.txt` and an example, `src/game-logic.cpp`: survive 90 seconds against a wave of four chasers every 20, spawned around player 1, and heal every player a segment every ten.
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
| `sdk::Game` and the SDK | [game/sdk/](../../src/game/sdk/include/game/sdk/sdk.h) | What a project's logic is best written with — [sdk.md](sdk.md) |
| `GameLogic` | [game-logic.h](../../src/game/logic/include/game/logic/game-logic.h) | The interface underneath `sdk::Game`: `start` (tick 0, once), `tick` (every tick), `hashState`, and `end` (once, in the tick the run ends — the deployed game and the logic tests stop there, so its writes are dropped) |
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

This is `GameLogicWorld`, API version 8 — what the engine lets logic read and change. The **SDK** built on it — a `Game` base with event hooks, entity queries, per-entity data, timers, spawn patterns, dice — is [sdk.md](sdk.md); start there to write a game.

| Read | |
|---|---|
| `tick()` | The tick being simulated, from 0 |
| `input()` | Every player's quantised input this tick |
| `playerCount()`, `player(i)` | Slot, position, aim, health and full bar, up/down/out, and a `target` |
| `actorCount()`, `actor(i)` | The level's id for it (the prop it was placed as), position, facing, faction, health, the state of its behavior it is in, and a `target` |
| `outcome()` | Playing, won or lost |
| `actorOf(target)`, `playerOf(target)` | The entity a kept target names now, or nothing when it is gone |
| `events()` | What happened in the last tick, in the order it happened: actors spawned, hurt, killed and removed; players hurt, downed, revived (`by` the teammate) and out; actors entering a state of their behavior (`state`, its id), noticing a target and attacking (`other`, whom). Each is noted where it happens — the actor passes note what an actor does as they decide it — every hit that takes health is its own event, however many land in a tick — so a dead actor's event carries where it fell and its name. A hurt, death or downing carries `amount`, the health it took (no more than was left); `cause`, what kind of thing did it (`ATTACK`, `SHOT`, `BLAST`, `HAZARD`, `LOGIC`); and `by`, who is credited — the striker, shooter or spiller, or for a blast whoever killed the one that went off. A hit that takes nothing — on the dead, or on a player still in the grace after a hurt — is not an event |
| `lineOfSight(from, to)`, `walkable(at)` | Asked of the navigation grid, for an actor of the default size; false outside it |
| `obstacleCount()`, `obstacle(i)` | The level's solid props, as boxes |

| Write | |
|---|---|
| `damage(target, amount[, by])` | Queued; applied as a hit, credited to `by` when given |
| `heal(target, amount)` | Queued; up to a full bar |
| `endRun(outcome)` | The first ending stands |
| `moveTo(target, at)` | Queued; teleports a player or an actor, which forgets its path |
| `removeActor(target)` | Queued; out of the run without a death or its blast, reported `ACTOR_REMOVED` |
| `setActorState(target, state)` | Queued; into a state of its behavior by id, from its start. False for a state its behavior lacks |
| `setActorFaction(target, faction)` | Queued; onto another side |
| `fireShot(shot)` | Queued; a projectile flying from the next tick, striking props and the other side |
| `blast(blast)` | Queued; hurts everyone within its radius this tick |
| `spawnHazard(hazard)` | Queued; a pool biting the other side from the next tick |
| `spawnEnemy(archetype, at, id)` | Queued; one of the project's enemy archetypes — health, body, behavior, side, model and death blast from `enemies.data.json` — named `id`. False, and nothing queued, for an archetype the project lacks or with no room left |
| `spawnActor(spawn)` | Queued; an actor made to measure from a `LogicSpawn`: where, facing, behavior, side, health, id, model. False with no room left |
| `actorRoom()` | How many more can be spawned this tick |
| `random(bound)` | `0 … bound-1` from the logic's own stream, `LOGIC_RNG_STREAM`, so a draw added to the logic never shifts what actors roll |
| `log(message)` | Presentation: the editor's log and `get_playtest`'s `logic_log`, or the deployed game's output |
| `cue({.at, .sound, .effect, .gain, .scale, .reach})` | Presentation: a sound played and an effect shown where the playtest can — never state, never hashed, nothing a tick reads, dropped by the headless deployed game. `sound` is a sound slot (`combat.blast`, `step.boots.wood`) or a WAV or Ogg file under `assets/`, loaded the first time it is cued; `effect` is a particle preset (`smoke`, `fireball`, …) or a whole combat effect by its cue's name (`combat.blast`); `reach` `AT` is heard from where it is, `EVERYWHERE` alike anywhere. `get_playtest`'s `logic_cues` lists the last few; a name with no sound or effect is warned of once in the log |

**Events** are gathered as a tick runs — spawns and removals as the logic's writes are applied, hurts, deaths and downs read off the pools at its end — and handed over on the next, so they are state, carried and hashed in the `logic` section.

**Spawns** are applied after the tick's other writes, in the order made, and read from the next tick on; they are not hurt by the tick that made them. A run has a fixed room, `GameSetup::actor_capacity`, so the pools never grow mid-tick: a playtest with logic, and a deployed game with logic linked in, get `GAME_LOGIC_ACTOR_CAPACITY` (2,048), the level's own actors included. A run without logic keeps exactly its level's room, and hashes as it always did. The navigation grid covers the level's players and actors with a margin; an actor spawned far outside it has no floor to plan across. In a playtest, a spawned actor is drawn as a player is — its model, or the stand-in cylinder — shows in the AI overlay, and `get_playtest` lists it after the level's, marked `spawned`. Its footsteps are not heard yet.

Everything else — cueing effects and sounds, patrol routes for spawned actors — is §9's.

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

**The check.** Before the editor loads a new library, `simplish-logic-check` — built beside the editor — runs it in a process of its own: the open level as it stands, unsaved edits included, baked into `build/logic/check/`, played by a stand-in for ten seconds (`LOGIC_CHECK_TICKS`) — **twice**, each with a fresh instance, comparing every tick's hash. A crash there, an error, a check still running after 60 s — a loop that never ends — or a second run that differs from the first fails the build, with the reason in `get_build`'s `errors`, and the editor never loads that library. When both runs agree, the project's own tests run next, in the same process — each `SIMPLISH_LOGIC_TEST` ([sdk.md §10](sdk.md#10-testing-it)) on a fresh instance and its own level — and a failed one fails the build too, reported in `get_build`'s `tests` and as a diagnostic at the test's file and line. A difference is reported by tick and hash section: in `logic`, the logic's own state differs — a clock, `std::rand`, an unordered container, uninitialised memory, or a static that outlives a run; anywhere else, the logic changed the world differently. It catches what the logic does at the start of a run; a crash that needs later play still reaches the editor, whose process the logic shares.

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

- **Despawning** without a death, and **patrol routes** for spawned actors.
- **More reads** — props, the navigation grid, line of sight, projectiles — and **presentation cues** from logic (a sound, an effect).
- **Replays naming their logic.** A replay does not yet record which build of the logic it was made with, so one replayed against changed rules reports a divergence rather than refusing to start.
- **The rendered deployed client**, and assets in the deploy folder with it.
- **Stepping into logic in a debugger** works — the library is a Debug build with symbols — but nothing in the editor attaches one.
