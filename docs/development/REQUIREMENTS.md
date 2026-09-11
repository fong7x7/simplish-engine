# Simplish — Development Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.1
**Status:** Draft
**Last Updated:** 2026-08-22

---

## Overview

How Simplish is built, tested, and kept honest. Three gates stand between a change and `main`: it compiles clean, it passes tests including determinism, and it stays inside the performance budgets in [Engine §7](../engine/REQUIREMENTS.md#7-non-functional-requirements).

Where code goes is specified separately, in [Code Layout](code-layout.md) — all buildable code lives under `src/`, in packages that each carry `include/`, `src/`, and `test/`. Read that document before adding a package.

---

## 1. Toolchain

| Tool | Minimum | Notes |
|---|---|---|
| CMake | 3.25 | Presets required; in-source builds rejected by the root `CMakeLists.txt` |
| Clang | 16 | Primary compiler on macOS and Linux |
| MSVC | 19.36 (VS 2022 17.6) | Windows; Clang-cl also supported |
| Ninja | 1.11 | Default generator for all presets |
| clang-format | 16 | Config in `.clang-format` — 2-space indent, 80 columns, C++20 |
| clang-tidy | 16 | Config in `.clang-tidy` — `*` with a curated exclusion list |
| Catch2 | v3 | Fetched via `FetchContent` |

Compiler versions are pinned in CI. A local build with a different version is fine; a formatting or lint disagreement with CI is resolved in CI's favour.

---

## 2. CMake Module Layer

The root `CMakeLists.txt` includes seven modules from `cmake/`. All seven exist and the engine builds against them.

| Module | Provides |
|---|---|
| `SimplishPlatform` | `ENGINE_PLATFORM_DESKTOP`, `ENGINE_PLATFORM_MACOS`, `ENGINE_PLATFORM_WINDOWS`, `ENGINE_PLATFORM_LINUX`, `ENGINE_PLATFORM_PS5`, `ENGINE_PLATFORM_XBOX`, `ENGINE_ARCH_X86_64`, `ENGINE_ARCH_ARM64` |
| `SimplishRenderer` | `ENGINE_RENDERER` option and `ENGINE_RENDERER_RESOLVED` — the per-platform default and override validation in [Platform §3](../platform/REQUIREMENTS.md#3-rhi-backend-selection). `STUB` is valid on every platform |
| `SimplishCompilerOptions` | Warning level, `-Wall -Wextra` as errors in CI, exceptions and RTTI disabled, and the strict floating-point flags the determinism contract requires (`-ffp-contract=off`, `/fp:precise`, no fast-math) |
| `SimplishDependencies` | `FetchContent` declarations with pinned versions. Deliberately few: nlohmann_json, FreeType, stb, Catch2, and SDL3 on desktop. Audio and networking dependencies are added when those systems are written |
| `SimplishTarget` | `simplish_add_module` (OBJECT library with include dir and dependencies), `simplish_add_library` (STATIC library composed from modules), `simplish_add_test` (Catch2 executable registered with CTest, resolving the full OBJECT-library closure — see [Code Layout §10](code-layout.md#object-libraries-and-test-linkage)) |
| `SimplishCodeQuality` | clang-tidy and clang-format integration, `format` and `lint` targets |
| `SimplishPackaging` | CPack configuration per platform |

The `simplish_add_module` / `simplish_add_library` split exists so per-package test executables can link OBJECT modules directly and rebuild fast, while shipping binaries link one composed static library — the pattern the copied platform tests already rely on.

Each package declares its module and its test executable in its own `CMakeLists.txt`; a layer's grouping directory composes its packages into one static library. See [Code Layout §7](code-layout.md#7-cmake-per-package).

---

## 3. Presets

| Preset | Platform | Config | Renderer | Purpose |
|---|---|---|---|---|
| `debug` | Host | Debug | Default | Day-to-day development; assertions and tick hashing on |
| `release` | Host | Release | Default | Shipping build |
| `relwithdebinfo` | Host | RelWithDebInfo | Default | Profiling and performance gates |
| `headless` | Host | RelWithDebInfo | `STUB` | Determinism and simulation CI — no GPU |
| `asan` | Host | Debug | Default | Address and UB sanitizers |
| `opengl` | Desktop | Debug | `OPENGL` | Fallback-backend verification |
| `windows-cross` | Windows x86_64, built on macOS or Linux | Debug | `DX12` | Compile and link the Windows build without a Windows machine — see §3.1. Build only |
| `ps5` | PlayStation 5 | Release | `GNM` | Requires the private SDK overlay — not yet defined |
| `xbox-series-x` | Xbox Series X | Release | `DX12` | Requires the private GDK overlay — not yet defined |

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug
```

### 3.1 Building for Windows from macOS

`windows-cross` builds the whole tree — editor, every test executable, and the DX12 backend — as x86_64 Windows binaries. clang-cl compiles and lld-link links against the MSVC runtime and Windows SDK, laid out by [xwin](https://github.com/Jake-Shadle/xwin). It is how DX12 code gets a compiler to read it without a Windows machine. Nothing it produces runs on the host, so it has no test preset. Test discovery is deferred to `ctest` time (`PRE_TEST`) so the build never tries to run a `.exe`.

One-time setup: about 900 MB downloaded, and about 800 MB kept in `~/.xwin/sysroot`. Running xwin with `--accept-license` accepts Microsoft's license for the SDK and runtime.

```bash
brew install llvm lld xwin
xwin --accept-license --cache-dir ~/.xwin/cache --arch x86_64 splat --include-debug-libs --use-winsysroot-style --preserve-ms-arch-notation --disable-symlinks --output ~/.xwin/sysroot
```

Then:

```bash
cmake --preset windows-cross && cmake --build --preset windows-cross
```

The toolchain file is [`cmake/toolchains/windows-x64-clang-cl.cmake`](../../cmake/toolchains/windows-x64-clang-cl.cmake). Set `SIMPLISH_WINDOWS_SYSROOT` to use a sysroot somewhere other than `~/.xwin/sysroot`. clang-cl sets CMake's `MSVC`, so the build takes the same `/W4 /WX` branch of `SimplishCompilerOptions` a Windows machine does, and it catches the same MSVC-STL and Windows-SDK problems. It does not replace the Windows CI job in §7: it compiles with a different compiler from MSVC and runs nothing.

---

## 4. Code Quality

### 4.1 Formatting

`.clang-format` is authoritative: 2-space indent, 80-column limit, attached braces, C++20. Formatting is not a matter of taste and not a review topic — CI checks it, and a diff fails the build.

```bash
cmake --build --preset debug --target format
```

### 4.2 Static Analysis

`.clang-tidy` enables `*` minus a curated exclusion list. New code must be clean. Suppressions require a `// NOLINTNEXTLINE(check-name)` with a comment stating why on the line above — a bare `NOLINT` is a review rejection.

```bash
cmake --build --preset debug --target lint
```

### 4.3 Conventions

| Element | Convention |
|---|---|
| Files | kebab-case (`rhi-device-factory.h`, `projectile-pool.cpp`) — see [Code Layout §6](code-layout.md#6-file-naming) |
| Types | `PascalCase` |
| Functions and methods | `camelCase` |
| Member variables | `snake_case_` with trailing underscore |
| Constants and enumerators | `SCREAMING_SNAKE_CASE` |
| Namespaces | lowercase, short (`eng`, `eng::render`, `eng::client`) |
| Header guards | `#pragma once` |
| Public headers | Open with a `DESIGN SUMMARY` block: responsibilities, threading, invariants, integration points |

---

## 5. Testing

### 5.1 Layers

| Layer | What it covers | Where it runs |
|---|---|---|
| Unit | Every public API of every module. Catch2, no GPU, no filesystem beyond fixtures | Every platform, every commit |
| Determinism | A simulation scenario runs N times and across platforms; tick hashes must match bit-for-bit | `headless` preset, every commit |
| Replay corpus | Recorded runs replay to their original final tick hash | `headless` preset, every commit |
| Golden image | Reference frames rendered per backend, compared within tolerance | Per-platform, every commit |
| Integration | `sandbox/` drives the engine end to end: window, device, tick, draw, shutdown | Per-platform, every commit |
| Soak | A long headless run under full horde load, checking for leaks, drift, and unbounded growth | Nightly |
| Performance | Budget verification under a reproducible load — see §6 | Every commit, reference hardware |

### 5.2 Rules

- Tests live inside the package they cover: `src/<layer>/<package>/test/test_<unit>.cpp`. There is no top-level `tests/` tree — see [Code Layout §2](code-layout.md#2-why-tests-live-in-the-package)
- Every simulation system needs a determinism test, not only a unit test. A system with correct behaviour and non-deterministic ordering is a broken system
- Tests must not depend on wall-clock time, thread scheduling, or filesystem iteration order
- A bug fix lands with a regression test that fails without the fix
- Backend tests are excluded from the build when their renderer is not selected, along with the backend package itself
- Both per-package test executables (`<package>-tests`) and a combined per-layer executable for CI are produced
- A package landing with an empty `test/` folder does not pass review

### 5.3 Coverage

Every public engine and game API is covered. Coverage is measured but not used as a gate number — a determinism test that proves ordering is worth more than a percentage.

---

## 6. Performance Gates

The budgets in [Engine §7](../engine/REQUIREMENTS.md#7-non-functional-requirements) are enforced, not aspirational.

| Gate | Mechanism |
|---|---|
| Scenario | A fixed replay at full horde load — 2,000 enemies, 20,000 projectiles — run on the `relwithdebinfo` preset |
| Measurement | Per-phase tick timing and total frame time, median and 99th percentile over a fixed tick count |
| Reference hardware | A pinned CI runner class per platform; results are only compared within a class |
| Threshold | A phase exceeding its stated budget, or a 5% median regression against the baseline, fails the build |
| Baseline | Updated deliberately on `main` when a change is accepted as a legitimate cost, with the reason recorded in the commit |
| Reporting | Every run publishes a per-phase breakdown so a regression names its own subsystem |

**Built so far:** the horde half of the scenario. `./scripts/perf-gate.sh` builds `relwithdebinfo` and runs the hidden `[perf]` Catch2 cases — today `test_horde_budget.cpp` in `src/game/world`: 2,000 swarmers in a pillared arena closing on four scripted players, 120 ticks of warm-up then 600 timed, reporting each phase's median, 99th percentile and worst tick, and failing when the actors' median exceeds Engine §7's 2.5 ms. The case sits beside the code it measures and is hidden because a debug build's number means nothing; a non-hidden companion proves the same scene deterministic and that the horde closes in, so the benchmark cannot pass by standing still. Projectiles, the pinned runner class, a stored baseline and the 5% regression rule wait on projectiles and CI.

---

## 7. CI Matrix

| Job | Platform | Preset | Runs |
|---|---|---|---|
| Build + unit | macOS arm64 | `debug` | Every push |
| Build + unit | macOS x86_64 | `debug` | Every push |
| Build + unit | Windows x86_64 | `debug` | Every push |
| Build + unit | Linux x86_64 | `debug` | Every push |
| Format + lint | Linux x86_64 | `debug` | Every push |
| Determinism + replay | All three desktop OSes | `headless` | Every push |
| Golden image | Per platform (Metal, DX12, Vulkan, OpenGL) | `debug` | Every push |
| Performance gate | Linux x86_64, Windows x86_64 | `relwithdebinfo` | Every push |
| Sanitizers | Linux x86_64 | `asan`, `tsan` | Every push |
| Soak | Linux x86_64 | `headless` | Nightly |
| Packaging | All desktop | `release` | Tags |

Cross-platform determinism is the job that matters most: the same replay must produce the same tick hash on macOS, Windows, and Linux. A divergence there is a release blocker regardless of what else is green.

---

## 8. Git Workflow

Work follows the [compound engineering](compound-engineering.md) loop — plan, work, review, compound — and lands through review lanes decided by the paths a change touches.

- Feature branches off `main`; no direct pushes to `main`
- Every change lands via pull request with CI green
- Commits are scoped and their messages state *why*, not *what changed* — the diff already says what
- An architectural decision lands with its [ADR](../decisions/README.md) in the same pull request
- A change to a requirement lands with the requirements-document edit in the same pull request — and is a red-lane change ([Compound Engineering](compound-engineering.md#review-lanes))
- A non-trivial problem solved lands with its write-up in [`docs/solutions/`](../solutions/README.md)

---

## 9. Enforcement Summary

| Check | Local | CI | Blocks merge |
|---|---|---|---|
| Compiles clean, zero warnings | ✓ | ✓ | ✓ |
| clang-format clean | `format` target | ✓ | ✓ |
| clang-tidy clean | `lint` target | ✓ | ✓ |
| Unit tests pass | `ctest` | ✓ | ✓ |
| Determinism tests pass | `ctest --preset headless` | ✓ | ✓ |
| Cross-platform tick hash match | — | ✓ | ✓ |
| Golden images within tolerance | — | ✓ | ✓ |
| Performance budgets met | — | ✓ | ✓ |
| Sanitizers clean | optional | ✓ | ✓ |

---

*This document is a living spec. Update it when the toolchain, the gates, or the CI matrix change.*
