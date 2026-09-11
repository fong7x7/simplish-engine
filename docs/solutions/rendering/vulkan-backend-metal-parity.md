---
title: "Bringing the Vulkan backend to Metal parity: what the renderers silently assume"
category: rendering
tags: [vulkan, metal, moltenvk, rhi, parity, push-descriptors, image-layouts, glslang, vma, validation, sdl3, capture]
date: 2026-09-11
platform: macOS (MoltenVK), Linux
backend: Vulkan
verified: "vulkan preset builds editor and all tests; 1219/1219 ctest pass; GPU readback tests and a scripted editor session run with zero Khronos validation and sync-validation messages on MoltenVK 1.4.2; Metal debug preset 1208/1208"
---

## Problem

The Vulkan backend had never been compiled. It declared methods it never
defined, had no VulkanMemoryAllocator, turned every Metal-style binding call
into a no-op, and had none of the four built-in pipelines (GUI, mesh, skinned
mesh, outline) that the renderers ask for. The goal was for the GUI and mesh
renderers to drive it exactly as they drive Metal, and get the same picture.

## Root Cause

The renderers were written against the Metal backend. They rely on things
Metal does quietly, so none of it appears in the RHI's signatures:

| The renderers assume… | Metal gives it by… | Vulkan has to… |
|---|---|---|
| `setVertexStageBytes` / `setFragmentStageBytes` / `bindFragmentTexture` by slot | `setBytes`, `setTexture` on the encoder | bind through descriptors — here a push descriptor set over one shared layout, with the bytes in a per-frame upload ring (a skinned palette is 3840 bytes, far over the 128 push constants promise) |
| never calling `textureBarrier` | automatic hazard tracking | track every image's layout: attachments into attachment layouts at pass start, sampled images back to shader-read at pass end (the outline samples the depth the scene pass just wrote), the back buffer to PRESENT at `end()` |
| destroying a buffer the moment it is done with it (the GUI regrows its vertex buffer mid-stream) | command buffers retain what they encode | hold destroyed objects until the frame that could use them has retired |
| `initial_pixels` on `createTexture` just works | `replaceRegion` | upload through a staging buffer, and leave even an unwritten sampled texture in a sampleable layout |
| clip-space +Y is up | Metal's convention | a negative-height viewport (core since 1.1) |
| a draw that sets no viewport still draws | the encoder defaults to the full target | set viewport and scissor at `beginRenderPass` |
| a slot never set reads as nothing | unbound Metal slots are tolerated | write every binding a shader can read — a zeroed uniform buffer and a 1×1 white texture stand in |
| shaders come from source | `newLibraryWithSource` on MSL | compile GLSL with glslang at device creation (DX12 does the same with `D3DCompile`) |

It also needed setup that the old skeleton never had: Vulkan 1.3's
`dynamicRendering` and `synchronization2` enabled in the feature chain,
`VK_KHR_push_descriptor`, `VK_KHR_portability_subset` when the device
advertises it, the `ENUMERATE_PORTABILITY` instance flag (without which the
loader hides MoltenVK), and `SDL_WINDOW_VULKAN` on the window.

## Solution

`src/platform/render/backends/vulkan/` mirrors the DX12 port:
`vulkan-shared-layout.*` is `dx12-root-signature.*`, `vulkan-upload-ring.*` is
`dx12-upload-ring.*`, and `vulkan-builtin-pipelines.*` holds GLSL that mirrors
the MSL and HLSL line for line. `vulkan-image-transition.*` does layout
tracking, `vulkan-retired-object.h` holds deferred destruction, and
`vulkan-glsl-compiler.*` wraps glslang's C interface.

Traps hit on the way, each worth the 30 minutes:

- **`SDL_Vulkan_GetInstanceExtensions` segfaults** before any Vulkan window
  exists (it dereferences SDL's uninitialised video device). Check for a
  window before creating the instance.
- **SDL's Cocoa list includes `VK_KHR_portability_enumeration`**, but setting
  the matching instance flag is the application's job.
- **Per-frame "render finished" semaphores trip current validation layers**:
  the presentation engine can still be waiting on one when the next frame
  re-signals it. Use one per swapchain image.
- **Resetting the in-flight fence before `vkAcquireNextImageKHR`** deadlocks
  the next `beginFrame` if acquire fails. Reset after.
- **Capturing the back buffer after `present`** is a sync hazard. The image
  belongs to the presentation engine until it is acquired again. Metal reads
  the drawable afterwards; Vulkan cannot, so capture works only between
  `beginFrame` and `present`.
- **GUI text lost glyphs on Vulkan, with zero validation errors.** The GUI
  renderer writes frame-wide vertex numbers into its index buffer and then
  passed each batch's start twice: as `first_index` and as `vertex_offset`.
  Metal ignored `first_index` and OpenGL ignored `vertex_offset`, so each drew
  correctly by accident; Vulkan (and DX12) honour both, so every batch after
  the first drew the quads that followed it. The GUI now passes
  `vertex_offset = 0`, and Metal's `drawIndexed` turns `first_index` into an
  index-buffer byte offset. `test_gpu_gui_renderer.cpp` draws two GUI batches
  on whichever backend the build has and fails on either half of the bug.
  OpenGL still ignores `vertex_offset` (no caller passes one now).
- **Homebrew's validation layer "failed to load"** until
  `DYLD_FALLBACK_LIBRARY_PATH=/opt/homebrew/lib`, because the manifest names
  the dylib by bare filename. See
  [Development §3.2](../../development/REQUIREMENTS.md#32-running-vulkan-on-macos).

## Prevention

- The GPU tests in `test_vulkan_command_list.cpp` draw with each built-in
  pipeline and read the pixels back: clear colour, GUI solid and textured
  quads, +Y up, mesh ambient and texture, skinned identity joint, and the
  outline on a depth edge. Run them under validation with
  `VK_KHRONOS_VALIDATION_VALIDATE_SYNC=1` after touching the backend; that
  run is what caught the capture hazard, which every pixel check passed.
- Validation cannot see a draw that reads the wrong vertices. Pixel readback
  through the real renderers can: `support/gpu_test_context.h` builds the
  selected backend on a hidden window, so a `test_gpu_*.cpp` runs unchanged on
  Metal, Vulkan and DX12.
- When a renderer starts relying on a new Metal behaviour (a second texture
  slot, a new stage-bytes slot), it needs a binding in `vulkan-shared-layout.h`
  and a row in `dx12-root-signature.h`, or it silently draws with the stand-in.
- A GLSL change belongs in the MSL and HLSL too. The three are checked against
  one another only by these tests and by eye.
