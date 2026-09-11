# Simplish — Engine Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.1
**Status:** Draft
**Last Updated:** 2026-08-22

---

## 1. Overview

The engine is a static library (`simplish-engine`) providing game-agnostic infrastructure for an isometric horde shooter: a deterministic fixed-timestep simulation, a hybrid 3D-plus-sprite isometric renderer, spatial queries at horde scale, audio, input, GUI, content loading, and networking transport.

It is **platform-agnostic**: the concrete RHI backends, the desktop client, and the distributor services live in [`src/platform/`](../platform/REQUIREMENTS.md), which links this library. The engine never includes headers from any layer above it. One inherited exception is tracked in [Platform §1.2](../platform/REQUIREMENTS.md#12-platform-agnostic-engine): the GUI still links SDL3 for clipboard and cursor calls.

The engine's design target is a specific load, and every subsystem is sized against it: **1–4 players, ~2,000 active enemies, ~20,000 in-flight projectiles, 60 Hz simulation, 60+ FPS render, on mid-range 2016 desktop hardware.**

---

## 2. Goals

1. **Determinism first.** The simulation is a pure function of initial state, seed, and the ordered input stream. Same inputs, same bits, on every platform. This is a hard constraint, not a preference — see [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md).
2. **Horde scale.** Thousands of entities and tens of thousands of projectiles are the normal case, not the stress case. Data layout, iteration order, and allocation strategy are chosen for that load.
3. **Readable chaos.** The renderer's job is making a screen full of hazards legible. Depth policy, silhouette treatment, and effect budgeting serve readability before fidelity.
4. **Cheap co-op.** Determinism makes lockstep viable for four players. The engine ships the transport and session machinery; it does not ship rollback, prediction, or server reconciliation.
5. **Measured, not assumed.** Every subsystem in §6 has a stated budget, measured in CI on reference hardware. Regressions fail the build.

---

## 3. Platform & Tech Stack

### Target Platforms

| Platform | Minimum OS | Arch | Renderer | Notes |
|---|---|---|---|---|
| macOS | 13.0 (Ventura) | arm64, x86_64 | Metal | Primary development target; Metal backend already in-tree |
| Windows | Windows 10 (1909+) | x86_64 | DX12 | OpenGL fallback available |
| Linux | Ubuntu 22.04+ | x86_64 | Vulkan | OpenGL fallback available |
| PlayStation 5 | PS5 SDK (NDA) | arm64 | GNM | Distributor stubs present; backend not written — see [platform/REQUIREMENTS.md](../platform/REQUIREMENTS.md) |
| Xbox Series X | GDK + GDKX (NDA) | x86_64 | DX12 | Shares the DX12 backend with Windows |
| Headless (CI) | any | any | Stub | Deterministic simulation runs with no GPU — see [ADR-006](../decisions/ADR-006-headless-deterministic-ci.md) |

Console SDKs are NDA-gated and excluded from the public repository. See [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions) — console commitment is still an open question.

### Language & Standards

- **Language:** C++20, no exceptions, no RTTI ([ADR-001](../decisions/ADR-001-no-exceptions.md))
- **Build:** CMake ≥ 3.25 with per-platform presets
- **Floating point:** strict IEEE-754, no fast-math, no FMA contraction in simulation code. `-ffp-contract=off` on Clang/GCC, `/fp:precise` on MSVC. Simulation code must not call transcendental functions from libm — the engine provides deterministic replacements ([ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md)): `sinCosDegrees` in `engine/math` so far, the one the simulation has needed
- **SIMD:** permitted in *rendering and audio* paths only. Simulation code stays scalar unless the SIMD path is proven bit-identical across all target architectures
- **Warnings:** zero at `-Wall -Wextra` on every platform

### Core Dependencies

| Subsystem | Library | Platforms | Notes |
|---|---|---|---|
| Rendering — RHI | Custom (in-tree) | All | Abstract interface in `engine/render/`; backends in `platform/` |
| Rendering — Metal | Metal, MetalKit, QuartzCore | macOS | `src/platform/`; building |
| Rendering — Vulkan | Vulkan 1.3 | Linux, Windows | `src/platform/`; not yet built in CI |
| Rendering — DX12 | D3D12, DXGI 1.6 | Windows, Xbox | `src/platform/`; compiles and links from macOS through the `windows-cross` preset, never yet run; not yet built in CI |
| Rendering — OpenGL | OpenGL 4.6 | Desktop | Fallback; `src/platform/` |
| Windowing & input | SDL3 | Desktop | `src/platform/`, plus the GUI's clipboard/cursor path (see above) |
| Math | In-tree `engine/math` | All | No GLM: the engine owns its vector and matrix types so simulation math stays under the determinism contract |
| Images | stb_image / stb_image_write | All | Fetched by CMake; used by the GUI image loader and the RHI capture API |
| Sprite atlas packing | Offline tool (in-tree) | Build-time | Produces atlas + metadata consumed at runtime |
| Content | JSON data tables + in-tree schema validator | All | Hot-reload in debug builds; shipping builds compile generated C++ instead ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)) |
| Audio (desktop) | OpenAL Soft 1.23+ via `FetchContent` | Desktop | Behind `IAudioBackend` |
| Networking transport | ENet 1.3.x via `FetchContent` | All | Reliable-ordered channel for lockstep input frames |
| Logging | In-tree `engine/core` logger | All | Disabled in simulation hot paths in release builds |
| Testing | Catch2 v3 | All | 1,159 tests green on macOS/Metal, 1,123 on the headless stub |
| Packaging | CPack | All | Platform-native installers |

---

## 4. Simulation Model

### 4.1 Fixed Timestep

The simulation advances in fixed **16.667 ms (60 Hz)** ticks. Rendering runs decoupled at the display rate and interpolates between the two most recent simulation states. A frame may advance zero, one, or several ticks; the accumulator is clamped to a maximum of 4 ticks per frame to prevent a death spiral, and the shortfall is reported rather than silently absorbed.

Tick order is fixed and documented, because it is part of the determinism contract:

1. Drain the input command queue for tick `N`
2. Player controllers
3. Enemy AI and steering
4. Weapon fire resolution → projectile spawn
5. Projectile integration and collision
6. Damage resolution and death
7. Spawn director
8. Deferred destruction and index compaction
9. Tick hash (debug/CI builds)

### 4.2 Entity Storage

Entities live in **structure-of-arrays** pools, one pool per archetype family, indexed by a generational handle (`{index: u32, generation: u32}`). Iteration is linear over dense arrays; destruction is deferred to the compaction phase so indices are stable within a tick. No per-entity heap allocation occurs during a run — pools are sized from level metadata at load and grow only at explicit checkpoints.

Composition is by struct, not by an entity-component-system framework — see [ADR-004](../decisions/ADR-004-soa-pools-over-ecs.md).

### 4.3 Determinism Contract

- **RNG:** PCG32, with a separate named stream per system (`spawn`, `loot`, `crit`, `fx`). FX streams are explicitly excluded from the simulation hash so cosmetic variance cannot desync a session.
- **Iteration:** every container iterated during a tick has a deterministic order. Hash maps are never iterated in simulation code; where an associative lookup is needed, it is backed by a sorted dense array.
- **Time:** simulation code cannot read wall-clock time. `now()` is not linked into the simulation translation units.
- **Hashing:** debug and CI builds compute a 64-bit hash of all simulation state at the end of each tick. Divergence between peers, or between a replay and its recording, is detected at the tick it occurs and reported with the offending subsystem.

### 4.4 Replay

A replay is `{level id, content hash, seed, input stream}` — typically under 100 KB per ten minutes. Replays are recorded always, in every build, and written alongside crash reports. Playback runs the identical simulation path with rendering attached, supporting pause, step, speed multipliers, and free camera. Replay verification is a CI job: a corpus of recorded runs must reproduce their final tick hash on every platform, every commit.

---

## 5. Rendering Model

### 5.1 Camera and Projection

A **fixed orthographic camera** at zero yaw and 4:3 dimetric foreshortening — world +X runs straight across the screen at full scale, world +Y recedes down the screen at 0.75, and world +Z rises straight up at full scale. Tiles are axis-aligned 64×48 rectangles and vertical surfaces are seen face-on, the Stardew Valley viewpoint rather than the 45°-yaw isometric one ([ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-08-26-straight-on-projection)). The ratio is configurable at build time but constant at runtime; the camera translates and zooms, never rotates. Rotation is out of scope, and the renderer is allowed to depend on that.

Because the camera is fixed and orthographic, the engine gets several things cheaply: cell-stable culling, stable sprite footprints (no perspective foreshortening to correct for), and a screen-space depth ordering that follows directly from world position.

A **static mesh path** exists as a first slice of that camera's use: `render-mesh` reads Wavefront OBJ, uploads vertex and index buffers, and draws instances depth-tested against a `D32_FLOAT` target. Two things about it are worth knowing before building on it:

- **The pipeline is a backend builtin**, reached through `RhiDevice::tryCreateMeshPipeline`, exactly as the GUI pipeline is. `createShader` takes compiled bytecode and the project has no shader build step, so a backend embeds its own shader source or reports no pipeline at all. Metal, DX12 and OpenGL each embed MSL/HLSL/GLSL that shade alike; Vulkan returns false and draws no meshes, which is a gap to close before it can show geometry.
- **Depth is measured along the projection ray, not along world Y.** The camera is oblique (§5.1), so points collapse to one pixel along `(0, RISE, DEPTH)` rather than along the screen normal. `makeIsoViewProjection` derives the clip matrix from that; a conventional look-at would order geometry wrongly wherever two things overlap on screen.
- **A mesh can be skinned.** A rigged glTF model — skeleton, skin, animation clips — is loaded by `engine/gltf`, posed each frame by `engine/animation`, and drawn by `SkinnedMeshRenderer` in the same pass and against the same depth as static meshes, through a second backend builtin, `RhiDevice::tryCreateSkinnedMeshPipeline`. It is for the handful of characters the [ADR-003 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters) admits, not the horde, and posing is presentation that the simulation never reads. [animation.md](animation.md) has the details.
- **How meshes look is chosen per frame.** A `MeshStyle` (`mesh-style.h`) carries the look a game sets at run time: how many tones each light is flattened into (none, for smooth light), and the width and colour of an outline. Banding happens in the mesh shader. The outline is its own pass — `MeshOutlineRenderer`, through `RhiDevice::tryCreateMeshOutlinePipeline` — that reads the depth the scene pass wrote and lines wherever it bends sharply: every silhouette, and creases such as a box's edges. Under the orthographic camera, depth across any flat surface is linear in screen position, so its second difference is zero on a plane however tilted and large at an edge; that is the whole test. The two presets are `MESH_STYLE_SMOOTH` and `MESH_STYLE_CEL`. The style is presentation only and never reaches the simulation. Depth alone cannot see an edge between two surfaces at the same depth and slope; that would need a normal buffer, which the scene pass does not write.

### 5.2 Hybrid Geometry and Sprites

Terrain, structures, and large static props are **3D meshes**, drawn instanced with a real depth buffer. Characters, small props, and effects are **camera-facing billboarded sprites**. The two are interleaved in a single depth-sorted pass rather than layered, so a player sprite standing behind a wall mesh is occluded by it correctly and without a special case.

Sprites participate in depth by writing a per-pixel depth derived from their world footprint and the sprite's declared height ramp, with alpha-test cutout for hard edges. This is the load-bearing technical decision of the renderer — see [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md).

### 5.3 Draw Submission

| Pass | Content | Technique |
|---|---|---|
| Depth pre-pass | Static terrain and structures | Instanced, depth-only |
| Opaque geometry | Terrain, structures, large props | Instanced by mesh + material, sorted front-to-back |
| Sprite opaque | Characters, enemies, props | Batched by atlas page, alpha-test, depth-write |
| Outline | Silhouettes and creases of everything above | Full-screen, reads the depth those passes wrote; skipped when the `MeshStyle` has no outline |
| Projectiles | Bullets, tracers, trails | Single instanced draw per archetype from a GPU-resident buffer |
| Transparent FX | Explosions, smoke, glows | Back-to-front, additive and alpha, depth-test without write |
| Decals | Scorch, blood, impact marks | Projected onto the depth buffer, pooled and age-evicted |
| HUD / GUI | Retained-mode UI | Screen space, last |

Projectile rendering never round-trips through the CPU per-instance: the simulation writes positions into a persistently-mapped ring buffer and the draw is a single instanced call per archetype.

The outline sits after the opaque passes because it needs their finished depth, and before projectiles and effects so that nothing it draws lands on them: a hostile projectile keeps its reserved hue (§5.5) whatever style the game is drawn in. Any later full-screen filter that changes colour, rather than adding lines, belongs at the same point for the same reason.

### 5.4 Lighting

A clustered forward path sized for a dark, wet, post-collapse palette: one directional key light, plus up to 256 dynamic point and spot lights per frame, with muzzle flashes, fires, and explosions all contributing. Shadows are limited to a single cascaded map for the key light plus screen-space contact shadows — with a fixed camera and mostly-static geometry, most shadow work is cacheable and re-rendered only on structural change.

### 5.5 Readability Rules

Legibility is a rendering requirement, not an art note:

- Player and enemy silhouettes get a configurable rim treatment that survives a bright, cluttered background
- Hostile projectiles are drawn in a reserved hue band that no cosmetic effect may use
- An effect budget caps total additive coverage per frame; over budget, cosmetic effects are culled by importance, never gameplay-relevant ones
- Occluding geometry between camera and player fades to a silhouette rather than being culled outright

---

## 6. Core Systems

| System | Document | Milestone |
|---|---|---|
| Core (allocators, handles, logging, RNG streams, fixed clock) | `core.md`; PCG32 and the fixed-step clock in [simulation.md](simulation.md) | M0 |
| Deterministic simulation tick, entity pools, command queue | [simulation.md](simulation.md) | **Built** |
| Replay recording and playback | [simulation.md §5](simulation.md#5-replay) | **Built** (engine side) |
| Isometric camera, projection, depth policy | `rendering/isometric.md` | M0 |
| Mesh rendering (instanced terrain, structures, props) | `rendering/mesh.md` | M1 |
| Sprite system (atlases, 8-direction facing, animation clips, batcher) | `rendering/sprites.md` | M1 |
| Skeletal animation (skeletons, clips, crossfades and pose blending, glTF rigs, GPU skinning) for a handful of characters | [animation.md](animation.md) | **Built** — `engine/animation`, `engine/gltf`, and `render-mesh`'s skinned renderer; Metal, DX12 and OpenGL pipelines. Nothing in the game uses it yet |
| Lighting and shadows | `rendering/lighting.md` | M5 |
| Effects (GPU particles, decals, trails, screen shake) | `rendering/fx.md` | M5 |
| Spatial structures (uniform grid, spatial hash, tile grid, flow fields) | [spatial.md](spatial.md) | M2 — built for actors: `engine/spatial` builds a navigation grid from the level's solid boxes, with clearance per cell, line of sight, deterministic A* with an expansion budget, path smoothing, reachability, flow fields built a budget of cells a tick, and a neighbour grid rebuilt each tick by counting sort. Per-objective fields, incremental updates and the tile grid are not written |
| Projectile simulation (integration, swept collision, penetration, homing) | `physics/projectiles.md` | M2 |
| Collision and queries (character sweep, overlap, line-of-sight) | `physics/collision.md` | M2 — first slice built: `engine/physics` resolves an upright cylinder out of axis-aligned boxes, deterministically, and a static-box broadphase gathers the boxes near a mover so a horde does not test every box every tick. Sweeps and projectile queries are not written |
| Audio (`IAudioBackend`, spatialisation, voice stealing, ducking) | `audio.md` | M3 |
| Input (action maps, rebinding, gamepad, deterministic capture) | `input.md` | M0 — first slice built: `engine/input` holds actions, turns screen-relative movement into world directions through the camera's `MoveBasis`, and quantises them into a `PlayerInput`; key binding lives with whoever reads keys (the editor's playtest today). Rebinding and gamepads are not written |
| GUI framework (retained-mode, layout, text, theming, docking, markdown) | [gui/README.md](gui/README.md) | **Built** |
| Content pipeline (JSON tables, schema validation, hot-reload) | `content.md` | M3 |
| Networking (transport, lockstep session, input delay, desync detection) | `networking.md` | M6 |
| Dev console, CVars, profiler, trace capture | `debug.md` | M3 |

> System documents are written as each milestone opens. The table is the authoritative list of what the engine owns; an absent document means the system is not yet specified, not that it is unowned.

---

## 7. Non-Functional Requirements

Reference hardware for desktop budgets: **GTX 1060 / RX 580 class GPU, 4-core CPU, 1080p**.

### Frame Budget (60 FPS = 16.6 ms)

| Subsystem | Budget | Notes |
|---|---|---|
| Simulation tick (total) | ≤ 6.0 ms | The whole of §4.1's ordered phases |
| — Enemy AI and steering | ≤ 2.5 ms | 2,000 active enemies |
| — Projectile integration + collision | ≤ 2.0 ms | 20,000 in-flight projectiles |
| — Damage, death, spawn, compaction | ≤ 1.5 ms | |
| Render submission (CPU) | ≤ 3.0 ms | Culling, batching, command list build |
| GPU frame | ≤ 12.0 ms | Leaves headroom for present and OS |
| Audio mix | ≤ 1.0 ms | Off the main thread; budget is for the mixer thread |
| Everything else | ≤ 2.0 ms | GUI, input, streaming, telemetry |

### Scale and Capacity

| Requirement | Target |
|---|---|
| Active enemies | ≥ 2,000 simultaneously simulated and rendered at 60 FPS |
| In-flight projectiles | ≥ 20,000 simultaneously simulated and rendered at 60 FPS |
| Dynamic lights | ≥ 256 per frame |
| Draw calls | ≤ 1,500 per frame at full horde load |
| Skinned mesh instances | ≤ 16 per frame, one draw each — players, bosses, set pieces. A crowd is sprites ([ADR-003 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters)) |
| Players per session | 1–4 |

### Timing and Footprint

| Requirement | Target |
|---|---|
| Frame rate (desktop, reference hardware) | ≥ 60 FPS at 1080p under full horde load |
| Frame rate (desktop, modern hardware) | ≥ 144 FPS at 1440p under full horde load |
| Frame rate (consoles) | ≥ 60 FPS at 1440p |
| Frame time consistency | 99th-percentile frame time ≤ 1.5× median; no frame over 33 ms during normal play |
| Simulation tick determinism | Bit-identical tick hashes across macOS/Windows/Linux for the CI replay corpus |
| Startup time | < 3 s from launch to main menu on a desktop SSD |
| Level load | < 2 s from selection to playable |
| Memory (desktop) | < 1.5 GB RSS at full horde load |
| Memory (steady state) | Zero heap allocation during a tick after level load |
| Save/replay portability | Replays and saves are cross-platform compatible |
| Build time | Full clean build < 8 minutes on 8 cores |
| Test coverage | Every public engine API covered by unit tests; every simulation system covered by a determinism test |
| Compiler warnings | Zero at `-Wall -Wextra` on all platforms |

Budgets are enforced by the CI performance gate — see [Development REQUIREMENTS §6](../development/REQUIREMENTS.md#6-performance-gates).

---

## 8. Milestones

| Milestone | Scope |
|---|---|
| **M0 — Foundation** | **Done:** `cmake/` module layer and presets; the engine/platform split; `math`, `core`, `image`, `render`, `gui`, and `client` engine packages plus the platform factory, five backends, and desktop client, with tests green on macOS/Metal and the headless stub; a first editor slice ([Editor REQUIREMENTS](../editor/REQUIREMENTS.md)). PCG32 RNG streams and the fixed-step clock in `core` ([simulation.md](simulation.md)). **Remaining:** isometric camera and projection; a triangle, then a tile, on screen; CI matrix green on macOS arm64/x86_64, Windows x86_64, Linux x86_64 |
| **M1 — Simulation & Draw** | **Done, engine side** ([simulation.md](simulation.md)): deterministic 60 Hz tick with the fixed phase order; SoA entity pools with generational handles; tick hashing; replay record, encode, and verify. **Remaining:** replay playback with rendering attached; instanced mesh rendering; sprite atlas, 8-direction facing, animation clips, and the depth-interleaved sprite/mesh pass |
| **M2 — Projectiles & Space** | Uniform-grid and spatial-hash broadphase; isometric tile grid and flow fields; projectile archetypes, integration, swept collision, penetration, homing; character sweep, overlap, and line-of-sight queries; GPU-resident projectile draw |
| **M3 — Content, Audio, UI** | JSON data tables with schema validation and hot-reload; `IAudioBackend` with OpenAL Soft, spatialisation, voice stealing, combat ducking; retained-mode GUI with layout, text, and theming; dev console, CVars, and the frame profiler |
| **M4 — Playable Slice** | First end-to-end vertical slice: one hand-built level, one weapon, two enemy archetypes, a working wave, win and lose states. Game-side scope in [Game REQUIREMENTS](../game/REQUIREMENTS.md) |
| **M5 — Presentation** | Clustered lighting, cascaded key-light shadows, screen-space contact shadows; GPU particles, decals, projectile trails, screen shake; the readability rules of §5.5 implemented and tuned |
| **M6 — Co-op** | ENet transport; deterministic lockstep session with configurable input delay; join, drop, and rejoin handling; desync detection with diagnostic capture; 1–4 players end to end |
| **M7 — Console** | PS5 GNM backend, Xbox GDK integration, console clients, controller and haptics, platform save containers, certification passes. Gated on the open question in [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions) |
| **M8 — Game Complete** | Full weapon, enemy, and director content; run structure and progression; distributor integration (Steam, Epic); packaging and installers. See [Game REQUIREMENTS](../game/REQUIREMENTS.md) |
| **M9 — Editor** | Level authoring, encounter and wave scripting, in-editor playtest. See [Editor REQUIREMENTS](../editor/REQUIREMENTS.md) |

---

*This document is a living spec. Update it as engine-level decisions land; competing-architecture choices belong in [an ADR](../decisions/README.md).*
