---
title: "FMA contraction was never disabled, so arm64 and x86_64 would round simulation math differently"
category: determinism
tags: [floating-point, fma, fp-contract, clang, arm64, x86_64, compiler-flags, tick-hash]
date: 2026-09-10
severity: high
platform: [macOS, Linux, Windows]
component: cmake/SimplishCompilerOptions.cmake
symptoms: "Tick hashes agree between runs on one machine and diverge between an Apple Silicon Mac and an x86_64 machine on the same replay"
resolution_type: config
---

## Problem

The determinism contract ([ADR-002](../../decisions/ADR-002-fixed-timestep-determinism.md), Engine §3) requires `-ffp-contract=off` / `/fp:precise`, and [Development REQUIREMENTS §2](../../development/REQUIREMENTS.md#2-cmake-module-layer) says `SimplishCompilerOptions` sets it. It did not. The flag appeared nowhere under `cmake/`. This was found while building `engine/sim`, before any simulation code depended on it.

## Root Cause

Clang's default for C and C++ is `-ffp-contract=on`, which fuses `a * b + c` within one expression into a single fused multiply-add when the target has FMA. A fused multiply-add rounds once where the separate operations round twice, so the result can differ in the last bit.

- **arm64** always has FMA, so Clang fuses.
- **x86_64** here compiles with `-msse4.2 -mavx2` but not `-mfma`, so Clang cannot fuse.

The same source therefore produces different bits on the two architectures. A tick hash would agree run to run on one machine, which is what local determinism tests check, and disagree across machines, which only the cross-platform CI job catches. A lockstep session between a Mac and a PC would desync on the first contracted expression that affected state.

## Solution

`cmake/SimplishCompilerOptions.cmake`, applied to every target through `simplish_compiler_options`:

```cmake
if(MSVC)
    target_compile_options(simplish_compiler_options INTERFACE /fp:precise)
else()
    target_compile_options(simplish_compiler_options INTERFACE
        -ffp-contract=off
        -fno-fast-math
    )
endif()
```

It is global rather than scoped to simulation targets. Game code links engine headers with inline math, and a flag that has to be remembered per target will eventually be forgotten on one. The cost to rendering and GUI math is negligible next to that risk.

## Prevention

- Check a documentation claim against the build before relying on it. `grep -rn "fp-contract" cmake/` would have caught this at any point.
- The cross-platform tick-hash job ([Development §7](../../development/REQUIREMENTS.md#7-ci-matrix)) is the real guard, and it does not exist yet. Until it does, a Mac-only green run proves nothing about x86_64.
- If `-mfma` is ever added for rendering performance, `-ffp-contract=off` is what keeps simulation code safe. Do not remove one without re-reading this.
