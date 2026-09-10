---
title: "Cross-compiling the Windows/DX12 build from macOS with clang-cl and xwin"
category: build-issues
tags: [windows, dx12, clang-cl, lld-link, xwin, cross-compilation, toolchain, msvc-stl, llvm-rc, catch2, sdl3]
date: 2026-09-10
platform: Windows
backend: DX12
verified: "Full windows-cross build links (editor + 9 test executables); macOS debug build and all tests still pass"
---

## Problem

The DX12 backend had never been compiled. The only machine building the tree
was a Mac, and nothing Windows-specific gets built there. The goal was to
compile and link the Windows build on macOS without running it.

## Root Cause

This section covers two separate things: traps in the toolchain setup, and
bugs in the tree that stayed hidden because nothing had compiled it for
Windows.

**Toolchain setup:**

- By default, xwin names its library directories `x86_64`. clang-cl's
  `/winsysroot` and lld-link's `/winsysroot:` look for Microsoft's `x64`
  names. Lay the sysroot out with
  `--use-winsysroot-style --preserve-ms-arch-notation`.
- The default APFS volume is case-insensitive, so xwin's casing symlinks
  aren't needed. It disables them itself; `--disable-symlinks` makes that
  explicit.
- `llvm-rc` has no `/winsysroot`. The `.rc` files that SDL3 and FreeType ship
  can't find `windows.h` or `winresrc.h` unless the SDK's `um`, `shared` and
  `ucrt` include directories and MSVC's `include` directory are passed with
  `/I` in `CMAKE_RC_FLAGS_INIT`.
- `catch_discover_tests` defaults to `POST_BUILD`, which runs the test
  executable as part of the build. That can't work when cross-compiling. Set
  `CMAKE_CATCH_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST` in the preset.
- `brew install lld` pulls in `llvm` and upgrades it. `clang-format` is a
  separate formula, so formatting isn't affected.

**Bugs the Windows build exposed** (each one also breaks a real Windows build):

- `/Zc:preprocessor` was passed whenever `MSVC` was set. clang-cl sets `MSVC`,
  its preprocessor is always conforming, and it rejects the flag as unused,
  which is an error under `/WX`. Restrict the flag to
  `CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"`.
- Catch2 asks only for `cxx_std_14`. MSVC and clang-cl default to C++14, but
  the project forces `CATCH_CONFIG_CPP17_STRING_VIEW` on, so `std::string_view`
  is missing. Apple Clang defaults to C++17, which hides this on macOS. Fix it
  with `target_compile_features(Catch2 PUBLIC cxx_std_17)`.
- SDL 3.2.8's `SDL_endian.h` defines `_m_prefetch`, which clang 20 and later
  have as a builtin. SDL 3.2.28 and later guard it with `SDL_HAS_BUILTIN`.
  Bumped to 3.2.30.
- The D3D12MemoryAllocator pin `v2.1.0` was never a real tag. Only a Windows
  configure fetches it, so nothing had caught that. Pinned `v3.2.0`.
- libc++ vs MSVC STL:
  - `std::array`'s iterator is a raw pointer in libc++ but a class in the
    MSVC STL, so `const auto* it = std::find_if(arr...)` doesn't compile.
  - libc++'s `<string_view>` pulls in `<string>`; the MSVC STL's doesn't.
    `assert.h` used `std::string` without including it.
  - `json == std::string_view` is ambiguous under the MSVC STL. Compare with
    a `std::string`.
- The CRT has no `setenv`/`unsetenv` (use `_putenv_s`), and it deprecates
  `std::getenv`. Define `_CRT_SECURE_NO_WARNINGS`.
- The Winsock functions live in `ws2_32.lib`, and POSIX sockets don't need a
  library, so `platform/agent` never linked one.
- The DX12 helper headers (`dx12-texture-lookup.h`, `Dx12CommandList`) took a
  `Dx12Device::Impl&`, but `Impl` was declared private.
- The DX12 tests included private headers through a relative path that
  resolves to a directory that doesn't exist (`src/render/...`). The Vulkan
  tests still have the same broken paths.
- clang-cl sets `MSVC`, so it took the MSVC floating-point flag,
  `/fp:precise`. clang-cl still contracts `a*b+c` into an FMA under
  `/fp:precise`, and its `/arch:AVX2` turns FMA on, which breaks the
  ADR-002 rule against contraction without any error or warning. This was
  checked by disassembling a test function: `vfmadd213ss` with
  `/fp:precise`, `vmulss` + `vaddss` with the fix. clang-cl now gets
  `/clang:-ffp-contract=off /clang:-fno-fast-math` instead. Adding
  `-ffp-contract=off` alongside `/fp:precise` doesn't work: it triggers
  `-Woverriding-option`, an error under `/WX`. MSVC itself doesn't contract
  under `/fp:precise` since VS 2022 without `/fp:contract`.
- Newer clang enables `-Wmissing-designated-field-initializers` under
  `-Wextra`. It fires for aggregates whose fields have no default member
  initializer, so `EditorAsset::path` and `relative_path` now have `{}`.

## Solution

`cmake/toolchains/windows-x64-clang-cl.cmake` plus the `windows-cross` preset.
Setup and usage are in
[development/REQUIREMENTS.md §3.1](../../development/REQUIREMENTS.md#31-building-for-windows-from-macos).
A quick way to check the toolchain on its own:

```bash
clang-cl --target=x86_64-pc-windows-msvc -winsysroot ~/.xwin/sysroot /c t.cpp
lld-link /winsysroot:$HOME/.xwin/sysroot /machine:x64 t.obj d3d12.lib
```

To confirm the DX12 renderer is actually in the editor binary:

```bash
llvm-readobj --coff-imports build/windows-cross/src/bin/editor/simplish-editor.exe | grep Name:
```

It should list `d3d12.dll`, `dxgi.dll` and `D3DCOMPILER_47.dll`.

## Prevention

- Build `windows-cross` after changing anything under
  `src/platform/render/backends/dx12/`, or code that uses the standard library
  in ways libc++ forgives: transitive includes, `std::array` iterators, POSIX
  calls.
- A dependency pinned behind a platform condition is only fetched on that
  platform. Check that its tag exists (`git ls-remote --tags`) when you pin it.
- Still not covered: running anything. The binaries are debug builds that
  import `ucrtbased.dll` and `MSVCP140D.dll`, which only machines with Visual
  Studio have. Nothing copies `SDL3.dll` next to the executables yet.
