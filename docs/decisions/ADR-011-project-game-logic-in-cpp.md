# ADR-011: A project's own game logic is C++ in its `src/`, loaded as a library to playtest and linked statically to deploy

**Status:** Proposed
**Date:** 2026-09-24
**Scope:** Game | Editor | Build

## Context

Content says what a game is made of; it cannot say every rule a game plays by. The data tables cover the things that recur — characters, behaviors ([ADR-009](ADR-009-actor-behavior-state-machines.md)), enemy archetypes, sounds — and the level's trigger logic is meant to cover declarative "when this, do that" ([project-format.md §7](../editor/project-format.md#7-logic-and-expressions)). What is left is the part of a game that is its own: when a level is won, what happens at the third minute, how a boss's second phase begins, a scoring rule, a mode. Today the only place such a rule can be written is `src/game/` in the engine repository, which makes every game a fork of the engine.

Developers and AI agents need to write that logic **in the game project the editor opens**, iterate on it with the editor's playtest, and ship it with the engine. Four existing decisions constrain how.

**Determinism is first ([ADR-002](ADR-002-fixed-timestep-determinism.md), Principle 1).** A project's rules run inside the tick, so they are simulation. Two lockstep peers must evaluate them identically, and a replay must remake them.

**ADR-007 rejected an embedded scripting language** (Alternative C): a VM in the tick brings float behaviour, table iteration order and garbage collection timing that vary. Whatever runs a project's rules must not reintroduce that.

**Playtest is instant ([ADR-007](ADR-007-json-authored-cpp-baked-content.md), Editor §7).** A designer presses Play and the game starts. A compile step is acceptable only where the author asked for one and the playtest does not wait on the engine being rebuilt.

**Shipping builds contain no loaders they do not need (ADR-007)**, and consoles do not allow a game to load code at run time.

## Decision

**A project's game logic is C++ in the project's own `src/` folder, written against one narrow engine interface. The editor compiles it into a shared library and loads a fresh copy for each playtest; a deploy compiles the same sources into the game's executable.**

- **One interface.** A project subclasses `eng::game::GameLogic` (`start`, `tick`, `hashState`) and exports it once with `SIMPLISH_GAME_LOGIC(Class)`. The world it sees is `eng::game::GameLogicWorld`: reads of the players and actors as plain values, and writes — damage, heal, end the run — that are queued and applied in order when the logic returns. Randomness is `GameLogicWorld::random`, on a stream of its own (`LOGIC_RNG_STREAM`) derived from the session seed.
- **It runs in one phase.** The director's (Engine §4.1 step 7): after damage, before compaction. Its writes go through the same functions an actor's bite does, so grace windows, deaths and death blasts behave identically.
- **It is simulation state.** A world with logic adds a last `logic` section to the tick hash: the outcome, the logic stream, and whatever `hashState` folds in. A world without logic hashes exactly as before, so existing replays still verify.
- **The boundary is an interface, not a link.** Every call from the logic into the world is a virtual call on an object the host made; the headers a project includes are header-only apart from engine/math, whose sources are compiled into the module. So a module links against nothing of the engine's, and the same object code is valid loaded into the editor or linked into a game.
- **One build file, two builds.** A project's `src/CMakeLists.txt` calls `simplish_game_logic(SOURCES …)` from `cmake/SimplishGameLogic.cmake`. Standalone (the editor's *Build Game Logic*) it makes `build/logic/game-logic.{dylib,so,dll}` with the engine's own compiler flags. Inside the engine (*Deploy Game*, `-DSIMPLISH_PROJECT_DIR`) it makes an OBJECT library that `src/bin/game` links in.
- **Same toolchain, checked version.** The editor builds a project's logic with the CMake, generator and compiler it was itself built with (baked in at the editor's build), because C++ vtables only agree between libraries one compiler laid out. A module reports `GAME_LOGIC_API_VERSION`; one that disagrees is refused at load, not called.
- **Hot, not live.** A build loads a copy of the new library for the *next* playtest; a running playtest keeps the library it started with until it stops. Nothing is swapped mid-run, so there is no state to migrate and no half-applied tick.
- **A fresh instance per run.** Every playtest, and every run of a deployed game, makes its own instance, so members start from their initialisers each time.
- **Deploy is static.** A deployed game has no dynamic loader for logic: the project's objects are linked into `simplish-game`, optimised with the engine under LTO.
- **Agents reach all of it.** `run_command` runs New Game Logic, Build Game Logic and Deploy Game; `get_build` reports the build, its log and the compiler's errors; `get_playtest` reports the outcome and what the logic logged.

## Alternatives Considered

### Alternative A: An embedded scripting language (Lua, Wren, AngelScript)

- **How it works:** Project rules are scripts the runtime interprets; the editor reloads them on Play.
- **Pros:** No compile step at all. Familiar to many designers. Safe to reload mid-run.
- **Cons:** Everything [ADR-007](ADR-007-json-authored-cpp-baked-content.md)'s Alternative C said: a VM's floating point, iteration order and collector timing inside a lockstep tick. It also ships an interpreter and readable source in every build, and a second language for agents and developers to be fluent in. The compile step it saves is two seconds (measured: a scaffolded project configures, compiles and loads in 2.4 s on an M-series Mac).

### Alternative B: Grow the closed data sets until they cover everything

- **How it works:** Every rule a game wants becomes a new condition, action or table column, the way ADR-009 grows behaviors.
- **Pros:** Designers never write code. One loader per table.
- **Cons:** Every rule becomes an engine change, reviewed and released with the engine — the fork problem again, in a different place. A win condition specific to one game has no business in the engine's vocabulary. The closed sets remain the right tool for what recurs; this ADR is for what does not.

### Alternative C: The copied C-ABI plugin host (`engine/core/plugin-*.h`)

- **How it works:** A table of C function pointers passed to `simplishPluginInit`, typed wrappers per domain, mod manifests, async tick threads.
- **Pros:** Already in the tree; a C ABI survives compiler mismatches.
- **Cons:** It was written for another project's voxel world — its domains are chunks, voxels and named entity components, none of which exist here — and it runs plugins outside the tick, on events and background threads, which is exactly what determinism forbids. A C ABI buys compiler independence the editor does not need, since it builds the module itself with its own compiler, at the cost of a wrapper for every call. `DynamicLibrary` is reused; the rest is left as it was.

### Alternative D: Logic in the engine repository's `src/game/`

- **How it works:** A game's rules are written where the built-in systems are, and the game ships as the engine's own executable.
- **Pros:** No boundary, no loader, no second build.
- **Cons:** Every game is a fork of the engine; engine updates become merges; the editor cannot rebuild a project's rules without rebuilding itself, which breaks the playtest loop. The engine's own systems stay there; a game's stay with the game.

### Alternative E: Ship the shared library too

- **How it works:** The deployed game loads `game-logic` at start, as the editor does.
- **Pros:** One loading path; logic patchable without relinking the game.
- **Cons:** Consoles forbid run-time code loading, so the static path is needed anyway, and a shipping loader is attack surface and a failure mode at start-up with nothing gained over linking. LTO cannot see across a library boundary. ADR-007 made the same call for content.

## Design Principle References

- **Principle 1: Determinism Always** — the logic runs inside the tick, in one fixed phase, draws only from a named stream, and is hashed; its writes go through the same ordered paths as every other system's.
- **Principle 4: Simplicity Over Flexibility** — one interface of plain reads and three writes, one phase, no reloading mid-run, no events, no threads. It grows by adding reads and writes deliberately, each tested.
- **Principle 5: Explicit Over Implicit** — the logic cannot reach the pools; everything it can do is a named method on `GameLogicWorld`, and every source it is built from is listed in its `CMakeLists.txt`.
- **Principle 6: Testability by Construction** — a logic is a class a test instantiates and steps in a `GameWorld` with no editor, no window and no library; the editor's own end-to-end test scaffolds, builds, loads and plays one.

## Consequences

### Positive

- A game's rules live with the game, in C++, and an agent or developer iterates on them with Build (seconds) and Play.
- The same sources and flags produce the playtest's library and the deployed game, so what was played is what ships.
- Replays and lockstep cover the project's rules, since they are hashed like everything else.

### Negative

- **The editor and the logic must share a compiler and standard library.** The version check catches a module built against other headers, not one built by another compiler against the same ones, which would load and then misbehave. The editor always builds the module itself, with its own toolchain, which is why this holds in practice; a hand-built one has to use the same compiler.
- **C++ in the tick can break determinism in ways the engine cannot catch** — a `std::unordered_map` iterated, a clock read, an address hashed. The rules are documented and the hash will show the divergence, but only after it happens.
- **A crash in the logic is a crash of the editor.** There is no sandbox; the library runs in-process. `simplish-logic-check` narrows it: every build is first run for ten seconds of the open level in a process of its own, and one that crashes or hangs there is never loaded. A crash that needs longer play still takes the editor with it.
- **The deployed game is headless today.** `simplish-game` runs the simulation with the project's rules and stand-in players — the dedicated-host and CI shape of Engine §5 — because the rendered client (`bin/client`, render-iso) is not written. It reads the data tables with the editor's readers until ADR-007's generator exists, so it links the editor library.
- **`GAME_LOGIC_API_VERSION` has to be bumped by hand** when a header under `game/logic/` changes shape.

### Implications for Future Work

- **More of the world** — reading props and the navigation grid, emitting presentation cues — joins `GameLogicWorld` as methods, each with a test and a version bump. Spawning joined in API version 2: the run's actor room is fixed up front (`GameSetup::actor_capacity`), so it stays a capacity decision made when the run starts rather than a pool that grows mid-tick.
- **The rendered client** links the same `simplish-project-logic` objects `simplish-game` does, and starts runs from the same baked setups.
- **Trigger logic** ([project-format.md §7](../editor/project-format.md#7-logic-and-expressions)) remains the declarative layer for designers. Where a trigger's action set cannot say something, project logic can; the two do not replace each other.
- **Replays should record which logic they were made with** — a hash of the library, or the sources — so one recorded against old rules is refused rather than reported as a divergence.
