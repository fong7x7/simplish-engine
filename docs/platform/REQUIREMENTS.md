# Simplish — Platform Layer Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.3
**Status:** Implemented — backends, client, and factory build and pass tests
**Last Updated:** 2026-08-22

---

## 1. Architectural Goals

`src/platform/` isolates every platform-specific implementation into a library of its own, so that `src/engine/` compiles once and links unchanged against any target.

### 1.1 Why It Is a Separate Library

The reason is build cost, not tidiness. Shipping the same game to several stores and platforms means building the same engine many times over. With the platform code inside the engine, changing a distributor or an RHI backend invalidates every engine object file. With it split out, a per-distribution build recompiles `src/platform/` only.

This is measurable: touching `rhi-device-factory.cpp` and `desktop-game-client.cpp` recompiles two translation units and relinks. Zero engine objects rebuild.

### 1.2 Platform-Agnostic Engine

`src/engine/` must compile with **no platform-specific includes** — no SDL3, no Vulkan, no Metal, no DX12, no console SDK headers. It depends only on the C++20 standard library, the project's few third-party libraries, and its own abstract interfaces (`RhiDevice`, `IAudioBackend`, `GameClient`).

> **Known exception.** `src/engine/gui/` links SDL3 for clipboard access and system cursor shapes. That is a genuine violation of this rule, inherited with the GUI framework. Closing it means routing both through a platform-utility interface the engine defines and `src/platform/` implements — the mechanism already exists as `DesktopPlatformUtility`. Until then, the engine is portable across desktop platforms but not free of SDL3.

### 1.3 Dependency Direction

```
bin/ ──► editor/ ──► platform/ ──► engine/
```

- `platform/` depends on `engine/` for `RhiDevice`, `RenderConfig`, `RhiCommandList`, resource types, and `RenderedGameClient`
- `engine/` depends on nothing above it
- `editor/` and `bin/` link platform, which transitively brings the engine

### 1.4 One Backend Per Binary

Exactly one RHI backend is compiled into any given binary, chosen at configure time by `ENGINE_RENDERER_RESOLVED`. Inactive backends are not compiled, and their tests are excluded from the build.

---

## 2. Current State

The platform layer builds and its tests pass: 114 tests on macOS/Metal, 66 on the headless stub backend.

### 2.1 What Is In-Tree

| Component | Location | Files | State |
|---|---|---|---|
| `RhiDeviceFactory` | `src/platform/render/` | 2 | Complete — compile-time backend selection, returns an empty optional on failure |
| Metal backend | `src/platform/render/backends/metal/` | 11 | Device, command list, format and resource maps, type converters; ARC-enabled `.mm` |
| Vulkan backend | `src/platform/render/backends/vulkan/` | 14 | Device, device init, command list, handle table, format map |
| DX12 backend | `src/platform/render/backends/dx12/` | 25 | Device, command list, handle table, descriptor heap allocator, frames-in-flight, resource wrappers, shared root signature, built-in GUI, static mesh and skinned mesh pipelines |
| OpenGL backend | `src/platform/render/backends/opengl/` | 26 | Device, command list, types, full set of `gl-cmd-*` command records |
| Stub backend | `src/platform/render/backends/stub/` | 2 | Headless `RhiDevice`; selectable via `ENGINE_RENDERER=STUB` — the CI determinism path |
| Desktop client | `src/platform/client/` | 8 | `DesktopGameClient`: SDL3 window, event pump, RHI creation, run loop, keycode mapping |
| Steam distributor | `src/platform/distributor/steam/` | 22 | Init, achievements, cloud, input, matchmaking, networking, overlay, workshop |
| Epic distributor | `src/platform/distributor/epic/` | 19 | Init, achievements, cloud, commerce, leaderboards, matchmaking, networking, overlay, social |
| PS5 distributor | `src/platform/distributor/ps5/` | 21 | Init, GNM config, Tempest audio, input, networking, save data, trophies, system |
| Xbox distributor | `src/platform/distributor/xbox/` | 26 | Init, DX12.x helpers and resources, audio, input, networking, save, sessions, achievements |

Every header follows the design-summary convention: responsibilities, behaviours, edge cases, invariants, and integration points stated at the top of the file.

### 2.2 What Remains

| Task | Detail | Milestone |
|---|---|---|
| Wire the distributors | All four distributor packages are stub implementations and are not yet in the build. They need `CMakeLists.txt` files and a soft-dependency policy per §4.3 | M7, M8 |
| Close the SDL3 leak | Route GUI clipboard and cursor calls through a platform-utility interface so `src/engine/` links no platform SDK at all (§1.2) | M1 |
| Backend audit | The backends were written for a voxel renderer. Audit resource sizing, descriptor budgets, and draw-call batching against sprite and projectile workloads — see [Engine §5.3](../engine/REQUIREMENTS.md#53-draw-submission) | M1 |
| Backend parity tests | Golden-image tests comparing Metal, Vulkan, DX12, and OpenGL output within a stated tolerance | M1 |
| Console backends | PS5 GNM backend and the Xbox GDK integration. Gated on the open question in [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions) | M7 |

---

## 3. RHI Backend Selection

`ENGINE_RENDERER_RESOLVED` is set by the `SimplishRenderer` CMake module and determines which backend module is built and linked. Exactly one backend is compiled in per binary; `RhiDeviceFactory::create()` returns an empty optional on failure rather than throwing ([ADR-001](../decisions/ADR-001-no-exceptions.md)).

| Platform | Default | Permitted overrides |
|---|---|---|
| macOS | `METAL` | `OPENGL`, `VULKAN` (MoltenVK), `STUB` |
| Windows | `DX12` | `OPENGL`, `VULKAN`, `STUB` |
| Linux | `VULKAN` | `OPENGL`, `STUB` |
| PlayStation 5 | `GNM` | — |
| Xbox Series X | `DX12` | — |
| CI (headless) | `STUB` | Selectable on any platform via `ENGINE_RENDERER=STUB` |

Backend headers are included only in `rhi-device-factory.cpp`, keeping platform SDK headers out of every other translation unit. When no concrete backend is selected, the factory module picks up the stub backend's include directory instead.

---

## 4. Requirements

### 4.1 RHI Backends

| ID | Requirement |
|---|---|
| PLT-RHI-1 | Every backend implements the complete `RhiDevice` and `RhiCommandList` interface with identical observable semantics |
| PLT-RHI-2 | Backend selection is compile-time; no runtime backend switching |
| PLT-RHI-3 | Device creation failure returns an empty optional with a logged diagnostic — never a crash, never an exception |
| PLT-RHI-4 | No platform SDK type appears in any public header of this layer |
| PLT-RHI-5 | Each backend has a Catch2 suite that runs without a GPU where the code path allows, and is excluded from the build when its renderer is not selected |
| PLT-RHI-6 | The stub backend supports a full headless simulation run, so determinism CI needs no GPU ([ADR-006](../decisions/ADR-006-headless-deterministic-ci.md)) |
| PLT-RHI-7 | Backends support the draw patterns the isometric renderer needs: instanced draws, persistently-mapped ring buffers for projectile data, and depth-interleaved sprite and mesh submission ([Engine §5.3](../engine/REQUIREMENTS.md#53-draw-submission)) |

### 4.2 Platform Clients

| ID | Requirement |
|---|---|
| PLT-CLI-1 | `DesktopGameClient` owns the window, the event pump, and the RHI device; it exposes no SDL type to `game/` or `engine/` |
| PLT-CLI-2 | Input events are translated to engine action-map events at the platform boundary, and the translation is deterministic — the same physical input sequence yields the same engine command sequence |
| PLT-CLI-3 | Input capture timestamps events by simulation tick, not wall clock, so recorded input replays exactly ([Engine §4.4](../engine/REQUIREMENTS.md#44-replay)) |
| PLT-CLI-4 | Window resize, focus loss, and display change are handled without disturbing simulation state |
| PLT-CLI-5 | Console clients implement the same `GameClient` contract; no game code branches on platform |

### 4.3 Distributor Services

Distributors are **soft dependencies**: the game builds and runs with none of them present, and every distributor call degrades to a no-op returning a well-defined default.

| ID | Requirement |
|---|---|
| PLT-DST-1 | No distributor SDK type appears in a public header |
| PLT-DST-2 | Distributor absence (client not running, invalid app ID, offline) is a supported state, not an error path |
| PLT-DST-3 | Callback pumping happens exactly once per frame on the main thread |
| PLT-DST-4 | Cloud save integration writes the same portable save format the desktop build uses |
| PLT-DST-5 | Distributor networking relays plug in behind the engine's transport interface, so co-op lockstep is unaware of which relay carries it |
| PLT-DST-6 | Achievement and stat IDs come from a data table, not from code |

| Service | Steam | Epic | PS5 | Xbox |
|---|---|---|---|---|
| Identity and presence | ✓ | ✓ | ✓ | ✓ |
| Cloud saves | ✓ | ✓ | ✓ (`SceLibSaveData`) | ✓ (Connected Storage) |
| Achievements / trophies | ✓ | ✓ | ✓ | ✓ |
| Networking relay | ✓ | ✓ | ✓ | ✓ |
| Matchmaking / sessions | ✓ | ✓ | ✓ | ✓ |
| Overlay | ✓ | ✓ | — | — |
| Input abstraction | ✓ (Steam Input) | — | ✓ (DualSense) | ✓ (GameInput) |
| Commerce / entitlements | — | ✓ | — | — |

---

## 5. Console Targets

PS5 and Xbox SDKs are NDA-gated and **excluded from the public repository**. The distributor headers in-tree define the interface surface; implementations live in a private overlay applied at build time on licensed machines.

Whether consoles are a real commitment is open — see [Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions). The layer is structured so the answer costs nothing either way: the interfaces exist, and an unimplemented distributor is already a supported state per PLT-DST-2.

---

## 6. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Recompile scope | A platform change recompiles `src/platform/` only; every `src/engine/` object file stays valid. Verified: touching two platform sources rebuilds two translation units and zero engine objects |
| Backend parity | A golden-image test renders identically across Metal, Vulkan, DX12, and OpenGL within a stated tolerance |
| Headless determinism | The stub backend produces bit-identical tick hashes to any GPU backend for the CI replay corpus |
| Distributor overhead | Callback pumping ≤ 0.1 ms per frame |
| Test isolation | Every backend suite builds and runs without its SDK present, or is cleanly excluded from the build |

---

*This document is a living spec. The `RhiDevice` interface these packages implement is specified in [Engine REQUIREMENTS](../engine/REQUIREMENTS.md).*
