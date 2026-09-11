# Simplish

Cross-platform C++20 game engine, editor, and (not yet written) horde-shooter game.
The documentation set is extensive and authoritative — this file is the routing
table into it plus the rules that are non-negotiable. Read the linked document
before working in an area; do not infer conventions from surrounding code alone,
because parts of `src/platform/` were copied in from another project and predate
these rules.

## Where to read

| Doing this | Read first |
|---|---|
| Anything, first time | [REQUIREMENTS.md](REQUIREMENTS.md) §5 architecture, §7 current state |
| Adding a package or file | [docs/development/code-layout.md](docs/development/code-layout.md) |
| Choosing between two designs | [docs/development/design-principles.md](docs/development/design-principles.md) — eight ranked principles; the higher rank decides |
| Proposing an architectural change | [docs/decisions/](docs/decisions/) — seven ADRs record what was already rejected and why |
| Build, tests, lint, CI | [docs/development/REQUIREMENTS.md](docs/development/REQUIREMENTS.md) |
| Engine, rendering, sim, netcode | [docs/engine/REQUIREMENTS.md](docs/engine/REQUIREMENTS.md) |
| Skeletons, animation clips, glTF rigs, skinned drawing | [docs/engine/animation.md](docs/engine/animation.md) — and the [ADR-003 amendment](docs/decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters) that limits it to a handful of characters |
| Enemies and NPCs: perception, behaviors, steering, attacks and damage, the Behavior row | [docs/game/actors.md](docs/game/actors.md) and [ADR-009](docs/decisions/ADR-009-actor-behavior-state-machines.md) — an actor's intelligence is a data state machine over closed sets, never a script |
| Navigation grid, line of sight, path planning | [docs/engine/spatial.md](docs/engine/spatial.md) |
| GUI: widgets, layout, text, docking, theming, markdown | [docs/engine/gui/README.md](docs/engine/gui/README.md) — one technical doc per subsystem, each naming its source files |
| Editor: authoring, viewport, assets, project format | [docs/editor/REQUIREMENTS.md](docs/editor/REQUIREMENTS.md), [project-format.md](docs/editor/project-format.md) |
| Adding **any** editor tool, panel, or command | [docs/editor/agent-api.md](docs/editor/agent-api.md) §6 — the same change exposes it to agents, and [capabilities.md](docs/editor/capabilities.md) records it |
| Driving the editor from an agent (MCP or HTTP) | [docs/editor/agent-api.md](docs/editor/agent-api.md) |
| RHI backends, windowing, distributors | [docs/platform/REQUIREMENTS.md](docs/platform/REQUIREMENTS.md) |
| Debugging something that smells familiar | [docs/solutions/](docs/solutions/) — problem/root-cause/fix write-ups, searchable by topic |
| Planning and review workflow | [docs/development/compound-engineering.md](docs/development/compound-engineering.md) |

Index of everything: [docs/README.md](docs/README.md).

## Architecture in one screen

Four layers, dependencies flow in exactly one direction:

```
bin/ ──► editor/ ──► platform/ ──► engine/
            └──────► game/ ──────────► engine/
```

- `src/engine/` — game-agnostic, platform-agnostic. **No platform SDK includes**
  (no SDL3, no Metal/Vulkan/DX12, no console headers), and never includes from a
  layer above it.
- `src/platform/` — every platform-specific implementation. The only layer that
  recompiles per target or per distribution; that is the whole point of the split.
- `src/editor/` — the desktop editor (`project`, `shell`).
- `src/bin/editor/` — thin `main()`; logic that deserves a test lives in a library.

Every package is `src/<layer>/<name>/{include,src,test}` — public headers under
`include/<namespace-path>/` matching the namespace (`include/engine/render/x.h`
→ `namespace eng::render`), private headers in `src/`, tests beside the code.
There is no top-level `tests/`, no root `include/`, no code outside `src/`.
Includes: `<angle brackets>` for public headers, `"quotes"` for a package's own
private headers, and never a relative escape into another package.

## Commands

```bash
./scripts/build.sh                  # configure + build (debug); --release, --headless, --tests
./scripts/test.sh                   # all tests; or scope: ./scripts/test.sh editor/shell
./scripts/editor.sh [PROJECT_DIR]   # build if needed, then run the editor
./scripts/format.sh                 # clang-format — CI fails on a diff
./scripts/lint.sh                   # clang-tidy + invariants, changed files vs main by default
./scripts/check-invariants.sh src/editor   # invariant checks alone, scoped to a path
./scripts/perf-gate.sh              # relwithdebinfo build, then the [perf] budget cases
```

Raw presets work too: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug`.
Presets: `debug`, `release`, `relwithdebinfo`, `headless` (stub RHI, determinism CI),
`opengl`, `vulkan` (runs on macOS through MoltenVK), `asan`.

## Invariants — enforced by scripts, not taste

`scripts/check-invariants.sh` fails on each of these. Run it on what you changed
before saying you are done.

| Rule | Detail |
|---|---|
| No exceptions, no RTTI | Both disabled at the compiler level ([ADR-001](docs/decisions/ADR-001-no-exceptions.md)). No `throw`/`try`/`catch`. Failure goes in the signature — `std::optional`, an error enum, a result type |
| Function bodies ≤ 16 lines | Non-blank, non-comment |
| ≤ 4 parameters | Including implicit `this` |
| No bare `bool` parameters | Use an `enum class` with two named values |
| One top-level type per header | Named for it, kebab-case filename |
| No forward declarations in headers | Include the real header |
| Header-defined bodies ≤ 1 operation | Templates, `constexpr`, `consteval` exempt; anything longer goes in a `.cpp` |
| No bare `new`/`delete` | Smart pointers or pool allocation |
| Every struct/class field carries a `///` comment | |
| Leading return types | `int foo()`, not `auto foo() -> int` (lambdas and deduction guides excepted) |
| No commented-out code | |
| An editor capability is reachable by an agent | A tool added to the editor is added to `src/editor/agent/` in the same change. `static_assert`s and `test_agent_tool_info.cpp` catch most of it; [agent-api.md §6](docs/editor/agent-api.md#6-adding-a-tool--the-rule) is the checklist |

Existing violations live almost entirely in the copied `src/platform/` code
(distributor stubs, RHI backends). Leave that backlog alone unless the task is
to fix it; the bar applies to code you write.

Suppressions need `// NOLINTNEXTLINE(check-name)` with a reason on the line
above. A bare `NOLINT` is a review rejection.

## Style

| Element | Convention |
|---|---|
| Files | kebab-case (`rhi-device-factory.h`, `entity-pool.cpp`) |
| Tests | `test_` + snake_case (`test_entity_pool.cpp`), one per unit under test |
| Types | `PascalCase` · Functions `camelCase` · Members `snake_case_` · Constants `SCREAMING_SNAKE_CASE` |
| Namespaces | lowercase and short — `eng`, `eng::render`, `eng::editor` |
| Header guards | `#pragma once` |
| Header doc block | New headers open with `/// @file`, `/// @brief`, and `/// @par Threading` (the pattern in `src/editor/`). Older copied headers use a `DESIGN SUMMARY` comment block — match the file you are in |
| Formatting | 2-space indent, 80 columns, attached braces. `.clang-format` is authoritative and not a review topic |

## Build-system gotchas worth knowing before you debug them

- Packages are CMake **OBJECT** libraries. CMake contributes objects only for
  OBJECT libraries named *directly* in `target_link_libraries`, so a transitively
  linked one gives you includes but not symbols. `simplish_add_test` walks the
  closure for you — a package test declares only its own module.
- `src/engine/render/` has no `src/` or `test/` on purpose: it is the abstract
  RHI interface, built as an INTERFACE library. Implementations and their tests
  are in `src/platform/render/`.
- Backend packages have no `test/` folder — their tests are in
  `src/platform/render/test/`, prefixed `test_metal_*`, `test_vulkan_*`, because
  the whole set is gated on the same `ENGINE_RENDERER_RESOLVED` condition.
- `src/engine/image/` exists so one translation unit owns the stb
  implementations for both layers. When two layers need the same
  single-definition TU, it belongs in the lower one.
- Distributor packages are unwired stubs — no `test/`, no `CMakeLists.txt` yet.
- List sources explicitly in `CMakeLists.txt`; `GLOB` is legacy only.

## State of the tree

Built and tested: engine `math`, `core`, `image`, `render`, `gui`, `client`,
`render-mesh` (static and skinned), `animation` and `gltf` (rigged models
posed by clips, for the few characters that are not sprites —
[docs/engine/animation.md](docs/engine/animation.md)), `sim` (tick, pools, hashing, replay —
[docs/engine/simulation.md](docs/engine/simulation.md)), `input` (held
actions to a quantised `PlayerInput`), `physics` (a first slice: cylinder
against boxes, and a static-box broadphase), `spatial` (navigation grid,
line of sight, A*, flow fields, a neighbour grid —
[docs/engine/spatial.md](docs/engine/spatial.md)); game `content`,
`player`, `combat`, `actors` and `world` (character and behavior
definitions, players moving on the tick at their character's speed, stopped
by props, hurt, downed and revived; actors that perceive, plan paths, move
and attack by their behavior, 2,000 of them inside the AI budget —
[docs/game/actors.md](docs/game/actors.md); projectiles, hazard pools and
blasts through an effects buffer the damage phase applies; stand-in
players; and the `SimulationSystems` composing them); platform
`render` (five backends), `client` (SDL3), `agent` (loopback HTTP); editor
`project`, `shell` (with the in-editor playtest) and `agent`; `bin/editor`.

Not written yet: the rest of `engine/spatial` (per-objective fields, the
tile grid), `render-iso`, `render-sprite`, `render-fx`, `audio`, `content`, `net`,
`debug`, the rest of `physics`, and everything in `src/game/` past
characters, players, actors and what actors' attacks do — weapons,
loadouts, the director, the run's structure. The isometric renderer is ahead, not behind — check
[REQUIREMENTS.md §6](REQUIREMENTS.md#6-repository--project-structure-target)
before assuming a system exists.

"Agent" means the AI driving the editor (`src/editor/agent/`); a thing in
the game that decides for itself is an *actor*, and its intelligence a
*behavior*. Keep the two words apart.

Determinism outranks everything else in the principle ranking: no wall-clock
reads, no hash-map iteration, no unordered parallelism, no fast-math in anything
that will feed the simulation ([ADR-002](docs/decisions/ADR-002-fixed-timestep-determinism.md)).
