# GUI Text — RHI Draw Path (No SDL Raster)

**Parent document:** [Technical approaches index](README.md)  
**Version:** 0.1  
**Last Updated:** 2026-03-20

---

## Overview

**GuiWidgets** draw text only through `GuiDrawContext`: `TextPipelineContext` rasterizes glyphs into a CPU RGBA atlas, uploads via `RhiDevice` when `gpu_device` is set, and `GuiRendererContext` emits textured quads with atlas UVs. `DesktopGameClient` owns an `RhiDevice` from `RhiDeviceFactory::create(RenderConfig)`; `initGui(..., device)` wires `text_pipeline->gpu_device`. The editor frame loop calls `beginFrame` → `renderAll` → `beginFrame` (RHI) → `endFrame` → `submit` → `present` instead of SDL triangle flush and deferred SDL text.

## Public Touchpoints

| API | Role |
|-----|------|
| `initGui(ctx, w, h, RhiDevice*)` | Sets `text_pipeline->gpu_device` before renderer init |
| `ensureGlyph` | Packs grayscale bitmap into atlas; `syncAtlasToGpu` recreates texture |
| `emitGlyph` | `emitTexturedQuad` with `GlyphInfo::atlas_texture` + UV bounds |
| `setFontRasterHeight` | `FT_Set_Pixel_Sizes` + clears glyph cache for that face |
| `RhiDeviceFactory::create` | `platform/render/src/rhi-device-factory.cpp`; stub via `platform/render/backends/stub/src/rhi-device-stub.cpp`; Metal/OpenGL entry points in `platform/render/backends/metal/src/` and `platform/render/backends/opengl/src/` |

## Review Log

| Cycle | Outcome |
|-------|---------|
| 1 | SDL font/present modules removed from draw path; atlas + stub RHI wired for desktop/editor |
