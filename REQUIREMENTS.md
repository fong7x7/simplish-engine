# Simplish — Project Requirements

**Version:** 0.1
**Status:** Draft
**Last Updated:** 2026-08-22

---

## 1. Project Overview

**Simplish** is a cross-platform isometric horde shooter and the engine that runs it, written in C++20. A run drops one to four players into a hand-authored slice of a collapsed modern world and buries them in enemies: hundreds of attackers on screen, thousands of projectiles in flight, and a fixed isometric camera that keeps the whole mess readable.

The name is a promise about the engine, not the game. Where a general-purpose engine generalises, Simplish commits: one camera model, one projection, one simulation cadence. The systems that survive that narrowing are the ones a horde shooter actually needs, built to be fast rather than universal.

---

## 2. Vision

Three pillars, in dependency order:

1. **Engine** — A horde-scale simulation and renderer. Fixed-timestep deterministic tick, data-oriented entity and projectile storage, GPU-instanced rendering through the existing RHI. Cross-platform on macOS, Windows, Linux, PlayStation 5, and Xbox Series X.
2. **Game** — The shooter itself: isometric movement and free aim, weapons and modifiers, enemy archetypes, wave directors, run structure, and 1–4 player co-op. Built on the engine, never inside it.
3. **Editor** — A desktop tool for hand-authoring levels, encounters, and wave scripts, with in-editor playtest against the real simulation. Desktop only.

Two constraints cut across all three and are non-negotiable:

- **Determinism.** The simulation is a pure function of `(initial state, seed, ordered input stream)`. This is what makes co-op netcode cheap, replays exact, and horde-scale bugs reproducible.
- **Budgets.** Every subsystem has a stated per-frame cost ceiling, measured in CI, and a regression is a build failure — not a note in a backlog.

---

## 3. Sub-Project Requirements

| Pillar | Document | Scope |
|---|---|---|
| Engine | [docs/engine/REQUIREMENTS.md](docs/engine/REQUIREMENTS.md) | Isometric render pipeline, entity & projectile simulation, spatial queries, deterministic tick, replay, audio, input, GUI, data-driven content, netcode transport, non-functional budgets, milestones M0–M7 |
| Game | [docs/game/REQUIREMENTS.md](docs/game/REQUIREMENTS.md) | Player controller and aiming, weapons and projectile patterns, enemy archetypes and horde AI, wave director, run structure and progression, co-op session rules, milestones M4–M8 |
| Editor | [docs/editor/REQUIREMENTS.md](docs/editor/REQUIREMENTS.md) | Editor shell, level authoring on the isometric grid, prop and entity placement, encounter and wave authoring, in-editor playtest, asset pipeline, milestone M9. Project format: [project-format.md](docs/editor/project-format.md) |
| Platform | [docs/platform/REQUIREMENTS.md](docs/platform/REQUIREMENTS.md) | Platform abstraction: RHI backend implementations (Metal, Vulkan, DX12, OpenGL, stub), `RhiDeviceFactory`, `DesktopGameClient`, distributor services (Steam, Epic, PS5, Xbox). **Largely already in-tree** — see §7 |
| Development | [docs/development/REQUIREMENTS.md](docs/development/REQUIREMENTS.md) | Toolchain, CMake presets, clang-tidy and clang-format gates, testing strategy (unit, golden-image, headless determinism), CI matrix, performance regression gates |

Cross-cutting references: [design principles](docs/development/design-principles.md) (ranked, and cited by every ADR) and [architectural decision records](docs/decisions/README.md).

---

## 4. Language & Build System

- **Language:** C++20
- **Build system:** CMake ≥ 3.25 with a preset per platform
- **Compiler targets:** Clang (macOS/Linux), MSVC or Clang-cl (Windows), platform toolchains for consoles
- **Exceptions and RTTI:** disabled — see [ADR-001](docs/decisions/ADR-001-no-exceptions.md)
- **Platform selection:** desktop platforms are auto-detected; `cmake --preset ps5` and `cmake --preset xbox-series-x` select console toolchains
- **Renderer selection:** `ENGINE_RENDERER` resolves to `METAL` on macOS, `DX12` on Windows and Xbox, `VULKAN` on Linux, with `OPENGL` as a desktop fallback and a stub backend for headless CI

Full toolchain, dependency table, and platform matrix: [Engine REQUIREMENTS §3](docs/engine/REQUIREMENTS.md#3-platform--tech-stack).

---

## 5. Architecture

Four layers, with dependencies flowing in exactly one direction:

```
bin/ ──► editor/ ──► platform/ ──► engine/
```

- **`src/engine/`** is a static library of game-agnostic, platform-agnostic infrastructure: rendering interfaces, simulation, spatial structures, audio, input, GUI, content loading, networking transport. It contains **no platform SDK includes** — no SDL3, no Vulkan, no Metal, no console headers — and never includes headers from any layer above it.
- **`src/platform/`** owns every platform-specific implementation: the concrete RHI backends, `RhiDeviceFactory`, the SDL3 desktop client, and the distributor services. It is **the only layer that recompiles per target or per distribution**.
- **`src/game/`** is all gameplay: weapons, enemies, the wave director, run rules, player state. Platform-agnostic; links the engine.
- **`src/editor/`** is a desktop application for authoring content, linking platform and engine.

The split between engine and platform is what makes per-distribution builds cheap. Building for a different store, console, or RHI backend rebuilds `src/platform/` only — engine object files stay valid, and a touched platform source recompiles two translation units rather than the whole tree.

`src/bin/sandbox/` links only the engine and serves as a minimal integration harness independent of any game design. `src/bin/server/` is a headless build of the simulation used for dedicated co-op hosting and for CI determinism runs.

## 6. Repository & Project Structure (Target)

All buildable code lives under `src/`, organised into packages. Every package has the same three folders — `include/` for public headers, `src/` for implementation and private headers, `test/` for its unit tests. Full rules in [Code Layout](docs/development/code-layout.md).

```
simplish/
├── src/
│   ├── engine/                 # Engine static library — platform-agnostic
│   │   ├── math/               # Vector, matrix, scalar math                    ✔ built
│   │   ├── core/               # Allocators, logging, event bus, init, plugins  ✔ built
│   │   ├── image/              # stb_image / stb_image_write implementation TUs ✔ built
│   │   ├── render/             # Abstract RhiDevice / RhiCommandList interface  ✔ built
│   │   ├── gui/                # Retained-mode UI framework                     ✔ built
│   │   ├── client/             # GameClient, RenderedGameClient                 ✔ built
│   │   ├── sim/                # Deterministic tick, SoA entity pools, replay   ✔ built
│   │   ├── spatial/            # Uniform grid, spatial hash, tile grid, flow fields
│   │   ├── render-iso/         # Isometric camera, projection, depth policy
│   │   ├── render-sprite/      # Sprite atlas, billboard batcher, animation clips
│   │   ├── render-mesh/        # OBJ+MTL loading, static and skinned draw   ✔ built
│   │   ├── animation/          # Skeletons, skins, clips, pose sampling     ✔ built
│   │   ├── gltf/               # Rigged glTF 2.0 models (.gltf, .glb)       ✔ built
│   │   ├── render-fx/          # GPU particles, decals, projectile trails
│   │   ├── physics/            # Cylinder vs box collision; projectiles, sweeps  ✔ built (first slice)
│   │   ├── input/              # Held actions → quantised PlayerInput           ✔ built (first slice)
│   │   ├── audio/  content/  net/  debug/
│   ├── platform/               # The only per-target / per-distribution rebuild
│   │   ├── render/             # RhiDeviceFactory + backends/                   ✔ built
│   │   │   └── backends/       #   metal, vulkan, dx12, opengl, stub            ✔ built
│   │   ├── client/             # DesktopGameClient (SDL3)                       ✔ built
│   │   └── distributor/        # steam/, epic/, ps5/, xbox/            — stubs, unwired
│   ├── editor/                 # Desktop editor — links platform + engine
│   │   ├── project/            # Project format, open/create, recent list       ✔ built
│   │   └── shell/              # Title bar, menu bar, toolbar, viewport, assets ✔ built
│   ├── game/                   # Gameplay — links engine
│   │   ├── content/            # Character definitions, the game's content     ✔ built
│   │   ├── player/             # Player pool and movement on the tick          ✔ built
│   │   ├── world/              # The game's SimulationSystems                  ✔ built
│   │   ├── weapons/  projectiles/  enemies/  director/  run/  coop/  — to write
│   └── bin/                    # Executables
│       ├── editor/             # The editor                                     ✔ built
│       └── client/  server/  sandbox/  audit-viewer/            — to write
├── cmake/                      # SimplishPlatform, SimplishRenderer, SimplishTarget, …
├── data/                       # Shipped content: tiles, sprites, weapons, waves, levels
├── tools/                      # Offline tooling: sprite atlas packer, content validators
└── docs/                       # This documentation set
```

Package boundaries are where the dependency rules in §5 are enforced: `src/engine/` never includes from a layer above it, and no package outside `src/platform/` includes a platform SDK header.

## 7. Current State

**The engine, the platform layer, and a first editor build and pass their tests** — 1,159 tests on macOS/Metal and 1,123 on the headless stub backend, with zero compiler warnings.

| Layer | Packages | State |
|---|---|---|
| Engine | `math`, `core`, `image`, `render`, `gui`, `client`, `render-mesh`, `animation`, `gltf`, `sim`, `input`, `physics` | Math, allocators, logging, event bus, engine init, expression evaluator, plugin host, audit system, PCG32 and the fixed-step clock; the deterministic tick, SoA entity slots with generational handles, per-subsystem tick hashing, and replay record/encode/verify ([simulation.md](docs/engine/simulation.md)); held input actions quantised into the `PlayerInput` a tick runs on; an upright cylinder resolved out of axis-aligned boxes; the abstract RHI interface (29 headers); a retained-mode GUI with layout, widgets, docking, theming, FreeType text, and a markdown renderer; `GameClient` / `RenderedGameClient`; rigged glTF models posed by animation clips, crossfaded between them, and skinned on the GPU, for the few characters that are not sprites ([animation.md](docs/engine/animation.md)) |
| Game | `content`, `player`, `world` | Players spawned from a `GameSetup` as the character each picked, taking its speed and health, moved by their stick on the deterministic tick, kept out of the level's solid props, and hashed; a determinism test and a replay round-trip over the whole world. Nothing else of the game yet |
| Platform | `render` (+ 5 backends), `client`, `distributor` | `RhiDeviceFactory`; Metal, Vulkan, DX12, OpenGL, and stub backends, one compiled in per binary; the SDL3 `DesktopGameClient`. Distributor packages are stubs, not yet wired into the build |
| Editor | `project`, `shell` | Project open and create against `.simplish/project.json` through a native dialog, a recent-projects list, and the editor shell: title bar, menu bar with dropdown menus, tool toolbar, a pan-and-zoom dimetric viewport, an asset strip whose models — OBJ, or rigged glTF that plays its animation clips — drag into the world as depth-tested 3D meshes, and click-to-select with a properties panel that moves and turns what is selected |

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

```bash
./build/debug/src/bin/editor/simplish-editor path/to/project
```

Dependencies are deliberately few — nlohmann_json, FreeType, stb, SDL3, and Catch2, all fetched by CMake. Nothing else is linked.

**What does not exist yet:** every engine package marked *to write* in §6, and the game past a moving player. The editor's Play button now drives the simulation tick with the game's world; the isometric renderer and the projectile system — the other systems this project is actually about — are M1 and M2 work that starts from the foundation above.

## 8. Open Questions

| # | Question | Blocks |
|---|---|---|
| ~~1~~ | ~~Isometric projection~~ **Closed 2026-08-26:** zero yaw, 4:3 dimetric — axis-aligned 64×48 tiles with an unforeshortened height axis (the Stardew Valley viewpoint), not a 45°-yaw isometric one. See [ADR-003 amendment](docs/decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-08-26-straight-on-projection) | ~~Art pipeline, sprite authoring~~ |
| 2 | Sprite source: hand-authored 2D, or 3D models pre-rendered to sprite sheets at fixed angles? The latter makes 8-direction facing cheap and keeps lighting consistent with the 3D terrain | Asset pipeline, M9 |
| 3 | Does the world have multiple height levels (stairs, elevated platforms, verticality), or is it a single floor plane with props? Verticality complicates occlusion, pathing, and depth policy considerably | Level format, editor tooling |
| 4 | Co-op transport: peer-to-peer lockstep, or always a listen server? Deterministic lockstep works for both; the choice affects NAT traversal and distributor relay integration | M6 |
| 5 | Is the run structure roguelite (procedural progression across authored levels, meta-unlocks) or campaign (fixed level order)? Levels are hand-authored either way | M8, save format |
| 6 | Console targets are declared but the SDKs are NDA-gated. Confirm whether PS5/Xbox are real M7 commitments or aspirational | Milestone plan, CI matrix |

---

*This is a living spec. Update it as project-level decisions land; sub-project detail belongs in the documents listed in §3.*
