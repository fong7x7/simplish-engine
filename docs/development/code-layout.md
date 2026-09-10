# Simplish — Code Layout

**Parent document:** [Development REQUIREMENTS](REQUIREMENTS.md)
**Version:** 0.1
**Last Updated:** 2026-08-22

Where code goes, and why. These rules are enforced in review and by the CMake module layer — a package that does not follow them will not build.

---

## 1. The Rule

**All buildable code lives under `src/`, organised into packages. Every package has the same three folders.**

```
src/
└── <package>/
    ├── include/     # Public headers — the package's API
    ├── src/         # Implementation and private headers
    └── test/        # Unit tests for this package
```

That shape is uniform: engine subsystems, game subsystems, platform backends, the editor, and the executables all follow it. There is no top-level `tests/` directory, no `include/` at the repository root, and no code outside `src/`.

A concrete package:

```
src/engine/sim/
├── include/
│   └── engine/sim/
│       ├── tick-scheduler.h
│       └── entity-pool.h
├── src/
│   ├── tick-scheduler.cpp
│   ├── entity-pool.cpp
│   └── tick-phase-table.h        # private — not in include/
└── test/
    ├── test_tick_scheduler.cpp
    └── test_entity_pool.cpp
```

---

## 2. Why Tests Live In the Package

Colocating tests with the code they test is the part of this layout that differs most from what the copied platform layer arrived with (a mirrored top-level `tests/` tree), so it is worth being explicit about the reasoning.

- **A package is one unit.** Moving, renaming, or deleting a package moves its tests with it. No second tree to keep in sync, and no orphaned test directory left behind by a refactor.
- **Coverage gaps are visible.** A `test/` folder with two files next to a `src/` folder with fifteen is obvious in a way that the same ratio spread across two distant trees is not.
- **Review is local.** A pull request touching one package touches one directory. The reviewer sees implementation and test changes side by side.
- **Build wiring is local.** Each package's `CMakeLists.txt` declares its module and its test executable together, so a new package is one file rather than an edit in two trees.

The cost is that a full-tree search for tests needs `src/**/test/` rather than one directory. This is what globs are for.

---

## 3. Package Boundaries

### 3.1 What Makes a Package

A package is the smallest unit that is independently buildable and independently testable. A directory is a package if — and only if — it contains the `include/` `src/` `test/` triple.

Split a package when it grows two independent responsibilities, or when part of it is needed by a consumer that has no business depending on the rest. Do not split it merely because a file is getting long.

### 3.2 Grouping Directories

Directories that only organise packages carry no code of their own:

```
src/
├── engine/                 # grouping — no include/, src/, or test/ here
│   ├── core/               # package
│   ├── sim/                # package
│   └── render/             # package
├── game/                   # grouping
│   ├── player/             # package
│   └── weapons/            # package
├── platform/               # grouping
│   ├── render/             # package
│   ├── client/             # package
│   └── distributor/        # grouping
│       ├── steam/          # package
│       └── epic/           # package
└── bin/                    # grouping
    ├── client/             # package (executable)
    └── server/             # package (executable)
```

The top level of `src/` is where layer boundaries live — `engine`, `game`, `platform`, `editor`, `bin` — and those boundaries carry the dependency rules in [Project REQUIREMENTS §5](../../REQUIREMENTS.md#5-architecture).

### 3.3 Nested Packages

A package may contain sub-packages in exactly one situation: **variant implementations of an interface the parent owns.** The RHI backends are the case this exists for:

```
src/engine/render/
├── include/                        # RhiDeviceFactory — the public API
├── src/                            # factory implementation
├── test/                           # factory tests
└── backends/                       # grouping
    ├── metal/{include,src,test}    # package
    ├── vulkan/{include,src,test}   # package
    ├── dx12/{include,src,test}     # package
    ├── opengl/{include,src,test}   # package
    └── stub/{include,src,test}     # package
```

Each backend is a real package with its own tests, built conditionally on `ENGINE_RENDERER_RESOLVED`. The parent owns the selection logic; the children own the implementations. Nesting for any other reason means the package should have been split into siblings instead.

### 3.4 Executables

An executable package has `src/` and `test/` but no `include/` — nothing links against a `main()`. Executables stay thin: argument parsing, configuration assembly, and a call into a library package. Logic that deserves a test belongs in a library, and the executable's own `test/` covers only its argument and configuration handling.

---

## 4. Include Paths

Only a package's `include/` directory goes on the public include path. `src/` is private and is visible only to that package's own translation units.

Inside `include/`, headers are nested by namespace path so that every include is unambiguous about what it is reaching for:

```
src/engine/render/include/engine/render/rhi-device-factory.h
                    └────┬───┘└───────┬────────┘
                    include root   namespace path
```

```cpp
#include <engine/render/rhi-device-factory.h>   // public header, angle brackets
#include "tick-phase-table.h"                   // private header, same package, quotes
```

| Rule | Detail |
|---|---|
| Public headers | `src/<package>/include/<namespace-path>/<file>.h`, included with angle brackets |
| Private headers | `src/<package>/src/<file>.h`, included with quotes, never referenced outside the package |
| No relative escapes | `#include "../../other-package/src/thing.h"` is never acceptable. If you need it, it belongs in the other package's `include/` |
| Namespace path matches directory | A header at `include/engine/render/x.h` declares in `namespace eng::render` |
| Platform SDK headers | Included only in `src/`, never in an `include/` header ([Platform §4.1](../platform/REQUIREMENTS.md#41-rhi-backends)) |

---

## 5. Test Layout

```
src/<package>/test/
├── test_<unit>.cpp        # one file per unit under test
└── support/               # fixtures, factories, and helpers for this package
```

| Rule | Detail |
|---|---|
| Naming | `test_<unit>.cpp`, matching the source file it covers (`entity-pool.cpp` → `test_entity_pool.cpp`) |
| One executable per package | `simplish_add_test` produces `<package>-tests`, linking the package's OBJECT module directly for fast incremental builds |
| Combined executable | CI also builds one executable over all test sources, per layer, from the same files |
| Shared fixtures | `test/support/` for helpers used across a package's tests. Helpers needed by *several* packages belong in a dedicated test-support package, not in a sibling's `support/` |
| Conditional tests | Tests for a conditionally-built package are excluded with it — the pattern the copied backend tests already use |
| Private access | Tests may include the package's private headers from `src/`. This is the one sanctioned exception to §4 |

---

## 6. File Naming

| Element | Convention | Example |
|---|---|---|
| Headers and sources | kebab-case | `rhi-device-factory.h`, `entity-pool.cpp` |
| Tests | `test_` + snake_case | `test_rhi_device_factory.cpp` |
| Objective-C++ | kebab-case with `.mm` | `metal-device-impl.mm` |
| Package directories | lowercase, single word where possible | `render`, `sim`, `distributor` |
| CMake files | `CMakeLists.txt`, one per package | |

One primary type per header, named for it. A header declaring `RhiDeviceFactory` is `rhi-device-factory.h`.

---

## 7. CMake Per Package

Every package carries a `CMakeLists.txt` declaring its module and its tests together:

```cmake
# src/engine/sim/CMakeLists.txt
simplish_add_module(simplish-engine-sim
    INCLUDE_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/include
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/src/tick-scheduler.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/src/entity-pool.cpp
    DEPENDENCIES simplish-engine-core
)

simplish_add_test(simplish-engine-sim-tests
    SOURCES      ${CMAKE_CURRENT_SOURCE_DIR}/test/test_tick_scheduler.cpp
                 ${CMAKE_CURRENT_SOURCE_DIR}/test/test_entity_pool.cpp
    DEPENDENCIES simplish-engine-sim
)
```

| Rule | Detail |
|---|---|
| Module naming | `simplish-<layer>-<package>` — `simplish-engine-core`, `simplish-engine-render`, `simplish-engine-render-backend-metal` |
| Test naming | The module name plus `-tests` |
| Composition | A layer's grouping directory composes its packages into one STATIC library via `simplish_add_library` |
| Explicit sources | List sources explicitly. `GLOB` is permitted only where the copied code already relies on it, and is not the pattern for new packages — a glob hides a file that failed to get added |
| Dependencies | Declared per package. A package that links something it does not use is a review rejection |

See [Development REQUIREMENTS §2](REQUIREMENTS.md#2-cmake-module-layer) for what these functions provide.

---

## 8. Outside `src/`

| Directory | Contents |
|---|---|
| `cmake/` | CMake modules — `SimplishPlatform`, `SimplishTarget`, and the rest |
| `data/` | Shipped content: tiles, sprites, weapons, waves, levels |
| `docs/` | This documentation set |
| `tools/` | Standalone offline tooling — the sprite atlas packer, content validators |

Nothing else at the repository root but the build files, the code-quality configs, and `REQUIREMENTS.md`.

---

## 9. Adding a Package

1. Create `src/<layer>/<name>/` with `include/`, `src/`, and `test/`.
2. Public headers go in `include/<namespace-path>/`, matching the namespace.
3. Write the `CMakeLists.txt` per §7, declaring both the module and its tests.
4. Add `add_subdirectory(<name>)` to the parent grouping directory's `CMakeLists.txt`.
5. If the package composes into a layer library, add its module to that layer's `simplish_add_library` list.
6. Write at least one test before the first implementation file is complete. A package that lands with an empty `test/` will not pass review.

---

## 10. The Source Tree Today

The layout above is not aspirational — it is what is on disk and what builds:

```
src/
├── engine/                         # platform-agnostic
│   ├── math/{include,src,test}
│   ├── core/{include,src,test}
│   ├── sim/{include,src,test}      # tick, entity slots, hashing, replay
│   ├── image/{include,src,test}    # stb implementation TUs
│   ├── render/include/             # RHI interface — headers only
│   ├── gui/{include,src,test}
│   └── client/{include,src,test}   # GameClient, RenderedGameClient
├── platform/                       # the only per-target rebuild
│   ├── render/
│   │   ├── {include,src,test}      # RhiDeviceFactory
│   │   └── backends/{metal,vulkan,dx12,opengl,stub}/{include,src}
│   ├── client/{include,src,test}   # DesktopGameClient (SDL3)
│   └── distributor/{steam,epic,ps5,xbox}/{include,src}
├── editor/
│   ├── project/{include,src,test}
│   └── shell/{include,src,test}
└── bin/
    └── editor/src/                 # main.cpp
```

Three deviations, all deliberate, listed here so nobody "fixes" them by accident:

- **`engine/render/` has no `src/` or `test/`.** It is the abstract RHI interface and nothing else, so `simplish_add_module` builds it as an INTERFACE library. Its implementations and their tests live in `platform/render/`.
- **Backend packages have no `test/` folder.** Their tests live in `platform/render/test/`, named by prefix (`test_metal_*.cpp`, `test_vulkan_*.cpp`), because a backend test needs the factory and the interface as much as the backend, and because the whole set is included or excluded by the same `ENGINE_RENDERER_RESOLVED` condition. Splitting them across five folders would mean five copies of that condition.
- **Distributor packages have no `test/` folder and no `CMakeLists.txt`.** They are stub implementations not yet wired into the build. When they are wired, they get both.

### Object Libraries and Test Linkage

Packages are OBJECT libraries, composed into `simplish-engine`, `simplish-platform`, and `simplish-editor` by `simplish_add_library`. CMake contributes object files only for OBJECT libraries named **directly** in a `target_link_libraries` call — a transitively linked OBJECT library propagates its includes and defines but not its objects, so a test that links one package and uses another through it fails at link time with undefined symbols.

`simplish_add_test` handles this: it walks `INTERFACE_LINK_LIBRARIES` from the declared dependencies and adds `$<TARGET_OBJECTS:...>` for every OBJECT library in the closure. A package test therefore declares only its own module and gets a working link.

### One Definition, Reachable From Both Layers

`engine/image/` exists for a linkage reason worth knowing. stb ships header-only, with implementations emitted by defining `STB_IMAGE_IMPLEMENTATION` in exactly one translation unit. Both the GUI (in `engine/`) and the RHI backends (in `platform/`) call into stb. Since `platform/` links `engine/` and not the reverse, neither could own the definition without the other losing the symbol — so a small engine-side module owns both TUs and both consumers link it.

The general rule: when two layers need the same single-definition translation unit, it belongs in the lower layer, not duplicated in the higher one.
