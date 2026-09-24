# Water — A Layer Over the Ground

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.2, §5.3, §5.4
**Packages:** `src/engine/render-water/` (`eng`), and the editor's `editor-water*`, `editor-graphics-*` and `simplish-editor-water.cpp` in `src/editor/shell/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) — presentation, never read by a tick; [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md) — drawn in the depth-buffered scene pass
**Status:** First slice built and tested. Water is a **layer of its own over the ground**, not one of its terrains: laid over grass, sand or a road, it leaves that terrain where it is, and the terrain shows through the water as far as the water is clear. Each cell of water has a **depth** — from a puddle a boot splashes through to a lake nobody sees the bottom of — a **colour** and an **opacity**, and all three blend from one tile to the next. It is drawn as a translucent, rippling surface **lit** by the lights the meshes are, stirred by drizzle, and rung by whoever wades through it and by shots and blasts that land in it. How much of that moves is the user's **water fidelity** — Flat, Low or High.

---

## 1. The Shape

```
  WaterLayer: depth, red, green, blue, opacity per cell — over the ground     water-layer.h
       │
       ├─ makeWaterCorners ──► every cell corner's water, read bilinearly      water-corners.h
       │
       ├─ makeWaterSurfaceMesh ──► the surface: the water's cells, autotiled,   water-surface-mesh.h
       │                           each vertex carrying depth, colour, opacity
       │
       └─ resetWaterField ──► WaterField: height, speed, depth per sample      water-field.h
                 │                  ▲
                 │   disturbWaterField (wakes, splashes, drizzle)
                 ▼
            stepWaterField(seconds)      the frame's clock, fixed 60 Hz steps
                 │
            writeWaterTexels ──► RGBA8: slope x, slope y, level, shore         water-texels.h
                 │
            WaterRenderer ──► tryCreateWaterPipeline, in the scene pass         water-renderer.h
                              after the opaque meshes, premultiplied over the
                              ground they drew, lit by their lights
```

| Piece | Header | What it owns |
|---|---|---|
| `WaterCell`, `WaterLayer` | `water-cell.h`, `water-layer.h` | One cell of water as bytes, and five grids of them over the level |
| `WaterFidelity` | `water-fidelity.h` | Flat, Low or High, their words, and the resolution each shapes at |
| `waterDepthTiles`, `waterDepthUnits`, `waterBankShelf` | `water-depth.h` | The unit depth is stored in, and how water shallows towards a bank |
| `WaterCorners`, `WaterSample`, `waterSampleAt` | `water-corners.h`, `water-sample.h` | The water at every cell corner, and read anywhere between them |
| `WaterField` | `water-field.h` | The height field: shaping it to the water, stepping it, pushing it, measuring it |
| `writeWaterTexels` | `water-texels.h` | The field packed into the texture the shader reads |
| `makeWaterSurfaceMesh`, `WATER_SURFACE_HEIGHT` | `water-surface-mesh.h` | The geometry the surface is drawn on, and where |
| `WaterLook`, `WaterShading`, `WaterVertexUniforms` | `water-look.h`, `water-shading.h`, `water-vertex-uniforms.h` | The sky, foam and clarity, and the blocks the shader reads |
| `WaterRenderer` | `water-renderer.h` | The pipeline, the surface's buffers, the field's textures, the draw |

---

## 2. The Layer

### A cell of water

A `WaterCell` is five bytes: `depth`, in `WATER_DEPTH_STEP`s (a sixteenth of a tile) from one step to `WATER_MAX_DEPTH` (almost sixteen tiles), with **0 meaning dry**; the water's sRGB `red`, `green` and `blue`; and its `opacity`, 0 to 255. A `WaterLayer` is five `GroundGrid`s of those bytes over the same cells, growing to hold whatever is laid as the ground's grid does; `setWaterCell` writes all five at once, and drying a cell clears every byte of it.

Water lives beside the ground rather than in it. The ground's grid holds a terrain for every cell, the water layer holds water for some of them, and neither knows about the other: laying water over a road leaves the road, and drying it again shows the road as it was. New water is `WATER_DEFAULT_DEPTH` (a pond, one tile) in `WATER_DEFAULT_RED/GREEN/BLUE` at `WATER_DEFAULT_OPACITY`.

### Blended between tiles

Nothing reads a cell's water directly. `makeWaterCorners` works out the water at every cell **corner** — the mean of the cells of water meeting there — and `waterSampleAt` reads between the four corners round a point: the depth bilinearly, and the colour and opacity weighted by depth as well, so a corner at a bank pulls the depth down without darkening the colour. A lake beside a pond, or a muddy inflow beside a clear lake, is therefore not a step at their shared edge but a blend across the tile between them.

### Shelving to the bank

However deep a cell is, its water is shallower at its bank. `waterBankShelf` eases from nothing at a bank to the full depth a run out from it, and the run grows with the depth — `WATER_BANK_MIN_TILES` plus `WATER_BANK_TILES_PER_DEPTH` a tile of depth, up to `WATER_BANK_MAX_TILES` — so a puddle is full depth almost at once and a lake shelves away over two tiles. The distance to the bank is the field's shore distance (§3), so the simulation and the shader shelve alike; every shader restates the function.

| Reader | Takes the water from | What it does with it |
|---|---|---|
| The field | `waterSampleAt` at each sample's depth, × the shelf | Wave speed and the bottom's drag (§3) |
| The surface mesh | `waterSampleAt` at each vertex | Carried to the fragment stage, blended by the rasterizer: depth in `uv.x`, opacity in `uv.y`, colour in the normal's place |
| The shader | That depth, × the shelf from the field's shore channel; that colour and opacity | How much of the ground the water hides, and with what (§4) |

---

## 3. The Simulation

A `WaterField` is a rectangle of samples over the water's bounds and a tile of dry land round them, `samples_per_tile` along each side of a tile. Each sample holds a **level** (how far it is above still water, in tiles), a **velocity**, and its **depth**. It is stepped by the shallow-water wave equation at a fixed `WATER_STEP_SECONDS` (1/60 s), in the velocity form:

```
velocity += pull × (sum of the four neighbours' levels − 4 × level)     pull = (√(g × depth) × step ÷ spacing)²
velocity ×= keep                                                          damping, per sample
level    += velocity
```

- **Depth sets the speed.** A ripple on water `d` tiles deep travels at `√(WATER_GRAVITY × d)` tiles a second — 2.5 on a default pond, 0.6 across a puddle, 4.3 across a three-tile lake. The pull is capped under `MAX_COURANT_SQUARED`, clear of the scheme's limit.
- **The bottom drags shallow water.** `WATER_BOTTOM_FRICTION` over the depth (no shallower than `WATER_FRICTION_MIN_DEPTH`) is added to the damping, so a puddle stills in a moment where a lake rolls on.
- **The shore is where the field is dry.** A dry sample has `keep` 0, so it never moves, and a ripple that reaches it reflects. The dry ring round the field means every wet sample has four neighbours inside it.
- **Beaches absorb.** Each wet sample's distance to the shore is measured once, when the field is shaped (a two-pass chamfer transform), and a sample within `WATER_BEACH_TILES` of it damps much harder (`WATER_BEACH_DAMPING`).
- **Ripples travel at the same speed at every resolution**; only the detail differs.
- **Drizzle.** `WATER_DRIZZLE_PER_TILE` small drops land a second on each tile of water, from the `WATER_RNG_STREAM` stream.
- **A long frame is cut short** at `WATER_MAX_STEPS_PER_FRAME` steps.

A push (`disturbWaterField`) lowers the water in a cosine bowl round a point, which rings out as it springs back. It touches only wet samples, and says whether it touched any.

### Why this is presentation

Nothing here is deterministic, and nothing needs to be: the field runs on the frame's clock, calls `exp`, `sqrt` and `cos`, and its drizzle comes from a stream no tick hashes. It reads the simulation's positions and cues after the ticks that made them, and nothing reads it back.

---

## 4. Drawing

### The surface

`makeWaterSurfaceMesh` builds the cells of water into a grid of their own and lets `makeGroundMesh` shape it, so the surface has exactly the ground's rounded ends, filled bends and joined diagonals. It lies flat at `WATER_SURFACE_HEIGHT` — ten ground layer steps, over every layer the ground stacks, so water lies on whatever terrain it was laid over, and under the thickness of the built-in tile shape, so a tile standing in water still stands out of it.

### The pass

The surface draws in the **scene pass**, after the opaque meshes, with its own builtin: `RhiDevice::tryCreateWaterPipeline`.

- **Over the real ground.** By the time it draws, the ground mesh has drawn the terrain under it; the water is blended over that.
- **Premultiplied.** The blend is `source + destination × (1 − alpha)`, the effects' blend. The water both hides part of the ground (its alpha) and adds light over the rest (the caustics and the glints), which straight alpha cannot do in one draw.
- **Depth tested, not written.** A crate standing in a pond hides the water in front of it, and the outline and the effects, which read the scene's depth after the pass, still see the ground under it.
- **One texture.** Vulkan's and DX12's shared layouts give a builtin one fragment texture slot, and the effects pass spends it on the scene's depth; drawing in the scene pass instead, against the real depth attachment, leaves the slot for the field.
- **Three blocks.** `WaterVertexUniforms` at vertex slot 1; `WaterShading` at fragment slot 0; and the scene's lights at fragment slot 1, as the very `MeshFragmentLights` block the mesh shader reads at its slot 0 — so the water is lit by the same list, the key light standing in when there is none, and cel-banded with the meshes.

### The texture

`writeWaterTexels` packs each sample into one RGBA8 texel: the slope along x and y (central differences, ÷ `WATER_SLOPE_RANGE`), the level (÷ `WATER_LEVEL_RANGE`), and the distance to the shore (÷ `WATER_SHORE_TILES`). The field is written into the next of `WATER_FIELD_TEXTURE_COUNT` textures each frame; they are created with still-water texels, which is what makes them CPU-writable on Metal.

### The shader

Per fragment, in all four backends' copies, in linear light:

| Term | What it does |
|---|---|
| Depth | The vertex's depth, shelved by the shore channel (§2) |
| Cover | How much of the ground the water hides: `1 − exp(−absorption × depth)`, the absorption running from `clear_absorption` at opacity 0 to `murky_absorption` at opacity 1, evenly on a log scale. Clear water over a tile shows most of the ground; opaque water hides it within a sixteenth |
| Colour | The cell's own colour, darkened by up to `deep_darkening` over `colour_depth` tiles, lit and covering the ground by `cover` |
| Ripples | The field's slope, drawn `WATER_RIPPLE_GAIN` times steeper than it is, so a wake a few hundredths of a tile high reads |
| Wind waves | Four directional waves travelling at deep water's speed for their wavelength; Low draws the two broad ones, High all four, Flat none. Calmer where the water is shallow |
| Sky | A Fresnel term, plus a term linear in how far a ripple's face is turned towards the eye (`WATER_RIPPLE_SKY`); it covers what it reflects over, so it hides the ground too, and is dimmed as the scene's light is |
| Caustics | Threads of light the waves focus on the ground, added over the part of it the water does not hide; fainter at Low, none at Flat |
| Lights | Every light in the block: ambient plus a Lambert term per light on the rippled normal, falling off as the meshes' point lights do and flattened into the same bands, lighting the colour and the foam |
| Glints | Each light's highlight off the wave faces, in that light's colour |
| Foam | A band along every shore, broken into lace, and — at High — on the crests a hard push raises |

The eye direction comes from the matrix's depth row, as the volumetric smoke's does ([fx.md §5](fx.md#5-volumetric-smoke)): the camera is orthographic and fixed, so it is the same for every pixel.

Metal, Vulkan, DX12 and OpenGL each embed their own copy, line for line; Vulkan's and OpenGL's GLSL share their fragment body word for word. DX12 compiles the vertex and pixel stages from separate sources, because each reads a `b1` of its own. A backend without the pipeline (the stub) draws no water.

---

## 5. Fidelity

| Fidelity | Simulated | Drawn |
|---|---|---|
| **Flat** | Nothing: shaped at `WATER_FLAT_SAMPLES_PER_TILE` (2) for its banks, never stepped, and nothing pushes it | A still surface: depth, colour, clarity, sky, lights, glints and shore foam, no waves and no caustics |
| **Low** | 4 samples a tile | The surface, with ripples, broad swell and faint caustics |
| **High** (default) | 8 samples a tile | All of Low, with finer wind waves, full caustics and foam on the crests |

Fidelity is a **graphics setting, not a property of the level**: the same water is laid at every fidelity, and a slow machine is slow in every project. The editor keeps it in the user's application data (§6).

---

## 6. In the Editor

- **Water and Dry cards.** General › ground holds the six terrains and Erase, then **Water** and **Dry**. The Tile tool's brush is a terrain, water or dry (`EditorGroundBrush`): water lays water over whatever the brush covers — only its depth where there was water already, keeping that body's colour and opacity, and the brush's colour and opacity where it was dry — and dry takes water off, leaving the terrain under it.
- **Depth.** With water as the brush, `,` and `.` step the depth it lays through `EDITOR_WATER_DEPTHS` — Puddle (1/16 tile), Shallows (1/4), Pond (1), Lake (3), Deep (8) — and the status line names it. Painting over water at another depth re-deepens it where the brush passes.
- **Bodies of water.** With the Select tool, a click on water selects the whole body — every cell of water joined to it, whatever its depth (`connectedWaterCells`) — before the ground under it. Its panel has a **Depth** row listing the named depths (naming a depth set by number, or a mix), and **Colour R**, **G**, **B** and **Opacity** as sliders: a new property kind, `SHADE`, a fraction from 0 to 1 drawn as a slider, which the lights' colour rows now use too. Each slider gesture is one undoable edit, and the brush takes the body's colour and opacity for new water, so painting more of a lake paints lake. Delete dries the body.
- **Undo.** A water edit is a ground paint (`PAINT_GROUND`) whose `water` list holds each changed cell's water on both sides (`EditorWaterChange`); a brush stroke carries its terrain and its water in one edit.
- **Saving.** `layers.water` — its own bounds and a run list per byte — beside `layers.terrain`, written only when there is water. A level from before water was a layer, which painted it as the terrain `tile:water`, is read as water in the default colour at its old depth, with the sand the old stacking drew under water given to the cell ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)).
- **Footsteps.** A step on a cell of water sounds like water, whatever terrain is under it.
- **The setting.** View › Water: Flat / Low / High, checked on the current one, kept in `graphics.json` in the user's application data.
- **Reshaping.** `EditorWater` rebuilds the field and the surface whenever the document's water layer or the fidelity differs from what it last shaped to.
- **Lights.** The water is drawn with the scene's light list and shading bands, the ones the meshes were drawn with that frame.
- **What pushes it.** While a level is played at Low or High, every player and actor that moves across water pushes a wake in proportion to how far it went (`EDITOR_WADE_*`); a shot that lands in it splashes (`EDITOR_SPLASH_*`); a blast over it heaves it (`EDITOR_BLAST_*`).
- **The clock.** The water ages on the frame's time while editing and playing, and stops with a paused playtest.
- **Agents.** `paint_water` lays or dries a rectangle, in a depth, colour and opacity; `get_ground`'s `water` rows show where it lies; `select` with `target` `water` picks a body, `set_property` sets its `color_r`, `color_g`, `color_b` and `opacity`, `set_water_depth` its depth, `delete` dries it, and `get_selection` reports all of them; `get_water` lists the named depths and what the water did last frame; `set_water_fidelity` and the three `set_water_*` commands set the fidelity ([capabilities.md](../editor/capabilities.md)).

---

## 7. Budget

| Cost | Budget | How it is held |
|---|---|---|
| Stepping the field and packing its texels, a frame | **≤ 1.0 ms** median, at `WATER_MAX_SAMPLES` | Water's share of Engine §7's 3 ms for render submission. `WATER_MAX_SAMPLES` is 512 × 512 samples — a 62 × 62-tile pond at High; wider water is simulated at half the resolution, and again, until it fits. Depth costs nothing a frame: each sample's pull and damping are worked out once, when the field is shaped. Flat steps nothing. Measured by the `[perf]` case in `test_water_field`, which `scripts/perf-gate.sh` runs: 0.77 ms on the development machine |
| Upload | One RGBA8 texture of the field a frame | 1 MB at the cap |
| GPU | One indexed draw, one texture read, eight lights and a handful of `sin`s per fragment | Only where there is water |

---

## 8. Testing

| Test | What it proves |
|---|---|
| `test_water_layer` | Water laid on a cell reads back, a dry cell reads as none, drying clears every byte |
| `test_water_depth` | Depths in steps with zero dry; the shelf from nothing at a bank to full depth, further for deeper water; depth and colour blending across the tile between two waters; a dry corner lowering depth without darkening colour |
| `test_water_fidelity` | Every fidelity's word names it back; Flat simulates nothing and shapes coarsest |
| `test_water_field` | The field covers the water and a dry ring; shore distance; each sample as deep as its cells, shelving to the bank; a ripple faster across a lake than a puddle, and a puddle stilling sooner; the budget coarsening then refusing; pushes; bounded at any resolution; a long frame cut short. `[perf]`: the budget above |
| `test_water_texels` | Still water packs level and unsloped, deep water reads deep and land dry, a hollow slopes up away from its middle |
| `test_water_surface_mesh` | The surface covers the water alone, over every ground layer; a lone cell is a round pool; each vertex carries depth, opacity and colour, blended between cells |
| `test_water_renderer` | Against a fake device: buffers and textures, one indexed draw with its three blocks, the field placed where the water is, fidelity's detail — Flat drawn still — the scene's lights handed through, textures written in turn, and everything handed back |
| `test_gpu_water_renderer` | On real Metal, and on Vulkan through MoltenVK, over a grey "ground": open water shows and the scene's depth hides it; clear water shows the ground where opaque water hides it; deeper water hides more; red water is red; a push changes what is drawn; High, Low and Flat draw differently; a red light reddens the water |
| `test_editor_water`, `test_editor_water_ops`, `test_editor_graphics_file`, `test_agent_water` | Reshaping and Flat's stillness; wading, splashing and aging; water as a layer and not a terrain; laying keeping a body's look; depth and look set on water only; undo; bodies; the Depth row; the water layer saved and read, and old levels' water migrated; footsteps on water; every agent tool and the three menu rows |

DX12 and OpenGL compile here but do not run; their shader text was checked with `glslangValidator` (GLSL 4.60, and HLSL through glslang's HLSL front end).

---

## 9. Not Yet

- **The ground under water is not refracted.** It is seen straight through the surface; ripples bend the light they focus on it, not where it appears.
- **Water does not know what stands in it.** A crate in a pond does not part the ripples: the field is shaped by the water's cells, not by props.
- **No shadows on the water**, and no reflections of the scene: the surface reflects a sky colour.
- **Eight lights a draw**, the meshes' budget, until the clustered light list exists ([fx.md §6](fx.md#6-flashes)).
- **Nothing in the simulation reads water.** Wading does not slow anybody however deep it is.
- **Flow.** The water stands; a river does not run.
