# Simplish — GUI Renderer: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.4](../gui.md)
**Library:** engine
**Date:** 2026-03-11; §2 rewritten 2026-09-25 for shapes, gradients, shadows and group transforms

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

## 2. Vertex Format and Shapes

All GUI quads use one 72-byte vertex, so any mix of shapes batches together. `gui-vertex.h` is the definition and `gui-vertex-layout.h` states the offsets. Every backend's vertex input restates them: Metal `makeGuiVertexDescriptor`, Vulkan `GUI_ATTRIBUTES`, OpenGL `setupGuiVertexArray`, and DX12 `GUI_INPUT_ELEMENTS`.

| Loc | Field | Type | Meaning |
|---|---|---|---|
| 0 | `pos` | float2 | Layout-space position |
| 1 | `uv` | float2 | Texture coordinate, or 0..1 across a shape |
| 2 | `color` | uint | RGBA8; a gradient's start |
| 3 | `color2` | uint | A gradient's end |
| 4 | `radii` | float4 | Corner radii: top-left, top-right, bottom-right, bottom-left |
| 5 | `border` | float4 | Border widths: top, right, bottom, left |
| 6 | `flags` | uint | `GUI_VERTEX_*` |
| 7 | `rect_w`, `rect_h` | float2 | The quad's size |
| 8 | `param` | float | Linear gradient angle (radians from pointing right), or shadow blur |

| Flag | Fragment shader |
|---|---|
| `GUI_VERTEX_TEXTURED` (0x2) | Sample the bound texture × colour: glyphs, images, nine-slice pieces |
| `GUI_VERTEX_SHAPE` (0x4) | Cover the rounded rect `radii` describe, anti-aliased |
| `GUI_VERTEX_LINEAR_GRADIENT` (0x8) | Fill from `color` to `color2` along `param` |
| `GUI_VERTEX_RADIAL_GRADIENT` (0x10) | Fill from `color` at the centre to `color2` at the edge ellipse |
| `GUI_VERTEX_SHADOW` (0x20) | Cover the shape inset by `param`, softened over `param` (a blurred box shadow) |
| any `border` > 0 | Cover only the ring between the shape and the shape inset by each side's width |

The shader works in the quad's own pixels: `p = (uv - 0.5) * rect`. It evaluates a signed-distance rounded rect whose radius is picked by the quadrant `p` falls in. Coverage is smoothed over `fwidth`, or over the blur for a shadow. **One copy of this maths exists per backend.** They are `gui_fs_main` in `metal-device-impl.mm`, `GUI_FRAG_GLSL` in `vulkan-builtin-pipelines.cpp`, `GUI_FRAGMENT_SHADER_GLSL` in `opengl-device.cpp` and `GUI_HLSL_SOURCE` in `dx12-builtin-pipelines.cpp`. The CPU reference is `src/gui-quad-shading.cpp`, which the software rasterizer runs. Change all five together, and:

- run `test_gpu_gui_renderer.cpp`, which reads back gradient, per-corner radius, per-side border and shadow pixels on the build's backend (Metal on a Mac, and Vulkan via MoltenVK with the `vulkan` preset);
- run `test_gui_software_rasterizer.cpp` for the CPU copy;
- validate the GL and HLSL text with `glslangValidator`, since those backends only compile-check here.

Widgets do not build vertices. They call `GuiDrawContext`:

| Call | Draws |
|---|---|
| `drawRect(GuiRectPaint)` | Fill or `GuiGradient`, `GuiCorners` radii, `Edges` border per side: CSS `background`, `border`, `border-radius` |
| `drawShadow(rect, radii, GuiShadow)` | CSS `box-shadow`: offset, spread, blur, colour. Draw it before the box |
| `drawBox(rect, GuiStateStyle, opacity)` | A themed widget's box: the theme's shadow for the style's `elevation`, then fill and border |
| `drawNineSlice(rect, GuiNineSlice, tint)` | A texture whose corners keep their size while the edges and middle stretch: CSS `border-image` |
| `drawRoundedRect`, `drawRoundedBorderRect`, `drawFilledRect` | The older uniform-radius shorthands |

```cpp
// A card: soft shadow, rounded top only, a gradient header strip, an accent rule.
ctx.drawShadow(card, GuiCorners::all(8), theme.shadow(GuiElevation::MID));
ctx.drawRect({.rect = card, .fill = theme.palette.surface_raised,
              .radii = GuiCorners::all(8)});
ctx.drawRect({.rect = header,
              .gradient = GuiGradient{.from = theme.palette.primary,
                                      .to = theme.palette.primary_pressed,
                                      .angle_degrees = 90},   // CSS angle: to the right
              .radii = GuiCorners::top(8)});
ctx.drawRect({.rect = body, .border = {0, 0, 0, 3},        // a left rule
              .border_color = theme.palette.primary});
```

### 2.1 Group transforms and opacity

`GuiRendererContext` carries a `transform` (uniform scale, then offset) and an `alpha_scale`. Every emitted vertex, including glyphs, and every scissor rect passes through them. Lengths (rect size, radii, borders, blur) scale too, so a scaled shape keeps its proportions. The tree's paint walk sets them per widget:

- `render_scale` scales the widget and its subtree about its centre, and `render_offset_x/y` moves them. Layout and hit testing ignore both, as they ignore a CSS transform's effect on flow. Use them for a press-in, a pop-in, or a slide. `GuiAnimProperty::RENDER_SCALE`, `RENDER_OFFSET_X` and `RENDER_OFFSET_Y` animate them.
- `opacity` fades the widget's own drawing, as before, and now also its **children**, through `alpha_scale`. It is per primitive rather than composited as a group, so overlapping translucent children show through each other while fading.

`beginFrame` resets both. Overlay components (`registerComponent`) are drawn outside the tree walk and do not inherit.

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

- Each frame's geometry goes into one host-visible vertex buffer (default 64K vertices, 72 bytes each) and one index buffer (6 indices per quad: 2 triangles).
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
