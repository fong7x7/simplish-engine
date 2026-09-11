# Simplish — GUI Renderer: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.4](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | Quad batching: coloured, textured, and rounded-rect quads merged into one vertex buffer | gui.md SS4.4 |
| R2 | SDF text rendering for resolution-independent text at arbitrary scale | gui.md SS4.4 |
| R3 | Scissor-rect stack for scroll containers and overflow clipping | gui.md SS4.4 |
| R4 | Z-sorted draw layers: game viewport, HUD, menus, editor, tooltips, modals | gui.md SS4.4 |
| R5 | <= 50 draw calls for typical in-game HUD; <= 200 for full editor | gui.md SS7 |
| R6 | < 1 ms CPU + GPU for in-game HUD | gui.md SS7 |

---

## 2. Vertex Format

All GUI quads use a single vertex format to maximise batching:

```cpp
struct GuiVertex {
  float pos[2];          // screen-space position
  float uv[2];           // texture coordinates (atlas UV or 0 for solid)
  uint32_t color;        // RGBA8 packed
  float corner_radius;   // 0 = sharp, > 0 = rounded rect
  float border_width;    // 0 = no border
  uint32_t flags;        // 0x1 SDF text, 0x2 textured, 0x4 rounded (Metal SDF path)
  float rect_w;          // quad width in layout px (rounded-rect SDF); 0 if unused
  float rect_h;          // quad height in layout px; 0 if unused
};
```

The fragment shader branches on `flags` and `border_width`: textured quad (atlas text), solid fill, rounded-rect fill (`0x4`), and **border rings** when `border_width > 0` (outer minus inset inner shape, sharp or rounded). Metal and OpenGL GUI pipelines implement the same analytic coverage.

---

## 3. Draw Commands

The renderer builds a list of draw commands during tree traversal:

```cpp
enum class DrawCommandType : uint8_t {
  kQuadBatch,    // batched quads sharing the same texture/scissor
  kPushScissor,  // push a scissor rect onto the clip stack
  kPopScissor,   // pop the top scissor rect
};

struct QuadBatchCmd {
  uint32_t vertex_offset = 0;
  uint32_t vertex_count = 0;
  uint32_t index_offset = 0;
  uint32_t index_count = 0;
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;  // 0 = no texture
};

struct ScissorCmd {
  Rect rect;
};

struct DrawCommand {
  DrawCommandType type = DrawCommandType::kQuadBatch;
  union {
    QuadBatchCmd batch;
    ScissorCmd scissor;
  };
};
```

---

## 4. Batching Strategy

### 4.1 Batch Rules

Consecutive quads are merged into a single batch when they share the same:

1. Texture handle (or both have no texture).
2. Scissor rect (same clip region).

A batch break occurs when either changes. The renderer sorts draw commands by z-layer first, then processes widgets in tree order within each layer.

### 4.2 Vertex Buffer

- Each frame's geometry goes into one host-visible vertex buffer (default 64K vertices, 40 bytes each) and one index buffer (6 indices per quad: 2 triangles).
- The renderer keeps `GUI_FRAME_BUFFER_COUNT` (3) such pairs in `GuiRendererContext::frame_buffers` and moves to the next pair on every `uploadFrame`. Every backend keeps two frames in flight, so the GPU can still be drawing from the previous frame's pair while the CPU writes this one; a single shared pair would be overwritten mid-draw (torn quads, wrong glyphs). The third pair covers a backend configured one frame deeper — the RHI does not report its depth. `uploadFrame` must therefore run once per device frame, after `RhiDevice::beginFrame`.
- A pair is mapped, written CPU-side, then unmapped before draw submission.
- If a frame overflows its pair, that pair alone is replaced by one twice the size; the old buffers are destroyed at once, which every backend tolerates while a frame in flight still holds them (Metal retains them, Vulkan defers destruction until the frame retires). `test_gui_renderer.cpp` covers the rotation, the growth and shutdown.

### 4.3 Texture Atlas Batching

SDF text glyphs share a single font atlas texture, so all text in the same scissor region batches into one draw call. Themed UI textures (icons, backgrounds) should be packed into a UI atlas for the same reason.

---

## 5. SDF Text Rendering

The fragment shader for SDF text:

1. Samples the SDF atlas at the glyph UV.
2. Computes the signed distance: `d = texture.r - 0.5`.
3. Applies smoothstep based on screen-space pixel size for anti-aliasing: `alpha = smoothstep(-spread, spread, -d)` where `spread = fwidth(d)`.
4. Multiplies by the vertex colour alpha for per-glyph opacity.

This produces sharp, anti-aliased text at any scale without re-rasterization.

---

## 6. Scissor Clipping Stack

Scroll containers and overflow regions push scissor rects:

```cpp
struct ScissorStack {
  Rect rects[MAX_SCISSOR_DEPTH];  // MAX_SCISSOR_DEPTH = 16
  uint32_t depth = 0;
};
```

- **Push**: intersect the new rect with the current top-of-stack. Push the intersection. Emit a `kPushScissor` draw command.
- **Pop**: restore the previous rect. Emit a `kPopScissor` draw command.
- **Intersection**: the effective scissor is always the intersection of all active rects in the stack.
- At RHI submission, each `kPushScissor` / `kPopScissor` translates to an `RhiCommandList::setScissor()` call.

---

## 7. Draw Layers and Z-Order

Widgets are grouped into layers for correct draw order:

```cpp
enum class DrawLayer : int32_t {
  kGameViewport = 0,
  kHUD = 100,
  kMenus = 200,
  kEditor = 300,
  kTooltips = 400,
  kModals = 500,
  kDevConsole = 600,
};
```

Within a layer, widgets are drawn in tree traversal order (pre-order). The renderer collects draw commands per layer, then submits layers in ascending order.

---

## 8. Render Flow

1. **Collect** -- Walk the widget tree in pre-order. For each visible widget:
   a. If the widget is a scroll container, push its scissor rect.
   b. Emit quads for the widget background (solid colour or rounded rect).
   c. Emit quads for text content (SDF glyphs from the text pipeline).
   d. Emit quads for borders, icons, or other decorations.
   e. After children, pop scissor if pushed.
2. **Sort** -- Group draw commands by z-layer.
3. **Batch** -- Within each layer, merge consecutive compatible quads into batches.
4. **Submit** -- Upload the vertex/index buffer. Issue RHI draw calls per batch.

---

## 9. Public Interface (`engine/gui/gui-renderer.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| initGuiRenderer | `bool initGuiRenderer(GuiRendererContext&)` | Create vertex/index buffers, load shaders |
| shutdownGuiRenderer | `void shutdownGuiRenderer(GuiRendererContext&)` | Release GPU resources |
| beginFrame | `void beginFrame(GuiRendererContext&)` | Reset draw command list, map vertex buffer |
| emitQuad | `void emitQuad(GuiRendererContext&, const Rect&, uint32_t color, float corner_radius, float border_width)` | Add a solid/rounded quad |
| emitTexturedQuad | `void emitTexturedQuad(GuiRendererContext&, const Rect&, const Rect& uv, RhiTextureHandle, uint32_t color)` | Add a textured quad |
| emitGlyph | `void emitGlyph(GuiRendererContext&, float x, float y, const GlyphInfo&, float scale, uint32_t color)` | Add an SDF text glyph |
| pushScissor | `void pushScissor(GuiRendererContext&, const Rect&)` | Push clipping rect |
| popScissor | `void popScissor(GuiRendererContext&)` | Pop clipping rect |
| endFrame | `void endFrame(GuiRendererContext&, RhiCommandList&)` | Batch, upload, submit draw calls |

---

## 10. Error Strategy

| Situation | Handling |
|-----------|----------|
| Vertex buffer overflow | Allocate overflow buffer; log warning |
| Scissor stack overflow (> 16) | Clamp to max depth; log error |
| Scissor pop on empty stack | No-op; log error |
| Invalid texture handle in batch | Skip batch; log error |
| Zero-size scissor rect | Skip rendering within rect; no draw commands emitted |

---

## 11. Edge Cases

- Widget fully outside the current scissor rect: culled during emit (no vertices generated).
- Transparent widget (alpha == 0): still emitted for hit testing but can be skipped in rendering with an early-out check.
- Empty frame (no visible widgets): beginFrame/endFrame cycle with zero draw calls.
- Multiple font atlases: each atlas causes a batch break; sorted to minimise breaks.

---

## 12. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/gui-renderer.h` | GuiVertex, DrawCommand types, GuiRendererContext, public functions | ~120 |
| `engine/gui/gui-renderer.cpp` | Batching, vertex buffer management, RHI submission, scissor stack | ~400 |
| `engine/gui/gui-shader.glsl` | Vertex + fragment shader (SDF, rounded rect, solid, textured) | ~80 |

---

## 13. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No specification of how the vertex buffer is double-buffered across frames to avoid GPU stalls while the CPU maps the buffer for writing (R6 performance target could be violated on some drivers)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Added clarification in section 4.2 that the vertex buffer uses per-frame ring allocation (two buffers alternated) so the GPU reads from the previous frame's buffer while the CPU writes to the current frame's buffer

### Final
**All checklist items pass.** Approach finalised.
