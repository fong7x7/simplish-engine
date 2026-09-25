# Water — A Layer Over the Ground

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.2, §5.3, §5.4
**Packages:** `src/engine/render-water/` (`eng`), and the editor's `editor-water*`, `editor-graphics-*` and `simplish-editor-water.cpp` in `src/editor/shell/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md) — presentation, never read by a tick; [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md) — drawn over the depth-buffered scene, from its depth
**Status:** Built and tested. Water is a **layer of its own over the ground**, not one of its terrains: laid over grass, sand or a road, it leaves that terrain where it is, and the terrain shows through the water as far as the water is clear. Each cell of water has a **depth** — from a puddle a boot splashes through to a lake nobody sees the bottom of — a **colour**, an **opacity** and a **flow**, and all four blend from one tile to the next. It is drawn over a copy of the scene: the ground seen through it bent by its ripples and fading red first and blue last into the water's own colour; the sky and what stands over it mirrored in it; the ground it has wet darker beside it, with waves lapping up that ground; foam rings where anything stands in it; wind waves, caustics and foam, **lit** by the lights the meshes are. It ripples — stirred by drizzle, rung by whoever wades through it and by shots and blasts that land in it, going round props that stand in it, carried downstream where it flows — and it splashes, throws V-shaped wakes, and holds foam that lingers and thins. How much of that moves is the user's **water fidelity** — Flat, Low or High.

---

## 1. The Shape

```
  WaterLayer: depth, colour, opacity, flow per cell — over the ground         water-layer.h
       │
       ├─ makeWaterCorners ──► every cell corner's water, read bilinearly      water-corners.h
       │
       ├─ makeWaterSurfaceMesh ──► a wet band a cell out, then the water       water-surface-mesh.h
       │                           autotiled, each vertex carrying its water
       │
       └─ resetWaterField(obstacles) ──► WaterField: level, speed, depth,       water-field.h
                 │                        flow, foam per sample; dry under
                 │                  ▲     what stands in it
                 │   disturbWaterField (wakes, splashes, drizzle)
                 ▼
            stepWaterField(seconds)      the frame's clock, fixed 60 Hz steps
                 │
            writeWaterTexels ──────► RGBA8: slope x, slope y, level, foam       water-texels.h
            writeWaterStillTexels ─► RGBA8: shore, flow x, flow y, land
                 │
   scene pass ends ─► WaterRenderer::captureScene: copyTexture(backbuffer)      water-renderer.h
                 │
            WaterRenderer::draw ──► tryCreateWaterPipeline, in the pass over
                                    the scene: field, scene copy, scene depth,
                                    still texels; lit by the meshes' lights
```

| Piece | Header | What it owns |
|---|---|---|
| `WaterCell`, `WaterLayer`, `waterCellFlow` | `water-cell.h`, `water-layer.h` | One cell of water as bytes, and seven grids of them over the level |
| `WaterFidelity` | `water-fidelity.h` | Flat, Low or High, their words, and the resolution each shapes at |
| `waterDepthTiles`, `waterDepthUnits`, `waterBankShelf` | `water-depth.h` | The unit depth is stored in, and how water shallows towards a bank |
| `WaterCorners`, `WaterSample`, `waterSampleAt` | `water-corners.h`, `water-sample.h` | The water at every cell corner, and read anywhere between them |
| `WaterObstacle` | `water-obstacle.h` | The footprint of something standing in the water |
| `WaterField` | `water-field.h` | The height field: shaping it to the water, stepping it, pushing it, carrying it downstream, measuring it |
| `writeWaterTexels`, `writeWaterStillTexels` | `water-texels.h` | The field packed into the two textures the shader reads |
| `makeWaterSurfaceMesh`, `WATER_SURFACE_HEIGHT` | `water-surface-mesh.h` | The geometry the band and the surface are drawn on, and where |
| `WaterLook`, `WaterShading`, `WaterVertexUniforms` | `water-look.h`, `water-shading.h`, `water-vertex-uniforms.h` | How the water takes the light, and the blocks the shader reads |
| `WaterSceneCopy` | `water-scene-copy.h` | What the scene is copied out of |
| `WaterRenderer` | `water-renderer.h` | The pipeline, the surface's buffers, the field's textures, the copy of the scene, the draw |
| `waterSplashEffect` | `water-splash.h` | The spray a splash throws, through the effects |

---

## 2. The Layer

### A cell of water

A `WaterCell` is seven bytes: `depth`, in `WATER_DEPTH_STEP`s (a sixteenth of a tile) from one step to `WATER_MAX_DEPTH` (almost sixteen tiles), with **0 meaning dry**; the water's sRGB `red`, `green` and `blue`; its `opacity`, 0 to 255; and its flow — `flow_heading`, in 256ths of a turn anticlockwise from east, and `flow_speed`, in 255ths of `WATER_MAX_FLOW_SPEED` (1.5 tiles a second), 0 for standing water. `waterCellFlow` turns the two into a velocity. A `WaterLayer` is seven `GroundGrid`s of those bytes over the same cells, growing to hold whatever is laid as the ground's grid does; `setWaterCell` writes all seven at once, and drying a cell clears every byte of it.

Water lives beside the ground rather than in it. The ground's grid holds a terrain for every cell, the water layer holds water for some of them, and neither knows about the other: laying water over a road leaves the road, and drying it again shows the road as it was. New water is `WATER_DEFAULT_DEPTH` (a pond, one tile) in `WATER_DEFAULT_RED/GREEN/BLUE` at `WATER_DEFAULT_OPACITY`, standing.

### Blended between tiles

Nothing reads a cell's water directly. `makeWaterCorners` works out the water at every cell **corner** — the mean of the cells of water meeting there — and `waterSampleAt` reads between the four corners round a point: the depth bilinearly, and the colour, opacity and flow weighted by depth as well, so a corner at a bank pulls the depth down without darkening the colour or stilling the current. A lake beside a pond, a muddy inflow beside a clear lake, a river turning a corner, is therefore not a step at their shared edge but a blend across the tile between them.

### Shelving to the bank

However deep a cell is, its water is shallower at its bank. `waterBankShelf` eases from nothing at a bank to the full depth a run out from it, and the run grows with the depth — `WATER_BANK_MIN_TILES` plus `WATER_BANK_TILES_PER_DEPTH` a tile of depth, up to `WATER_BANK_MAX_TILES` — so a puddle is full depth almost at once and a lake shelves away over two tiles. The distance to the bank is the field's shore distance (§3), so the simulation and the shader shelve alike; every shader restates the function. Flow slows the same way, from nothing at a bank to its full speed `WATER_FLOW_BANK_TILES` out.

| Reader | Takes the water from | What it does with it |
|---|---|---|
| The field | `waterSampleAt` at each sample, × the shelf | Wave speed, the bottom's drag, and the flow that carries it (§3) |
| The surface mesh | `waterSampleAt` at each vertex | Carried to the fragment stage, blended by the rasterizer: depth in `uv.x`, opacity in `uv.y`, colour in the normal's place |
| The shader | That depth, × the shelf from the still texels' shore; that colour and opacity; the flow from the still texels | How much of the ground the water hides, and with what, and which way its surface runs (§4) |

---

## 3. The Simulation

A `WaterField` is a rectangle of samples over the water's bounds and a tile of dry land round them, `samples_per_tile` along each side of a tile. Each sample holds a **level** (how far it is above still water, in tiles), a **velocity**, its **depth**, its **flow**, and the **foam** on it. It is stepped by the shallow-water wave equation at a fixed `WATER_STEP_SECONDS` (1/60 s), in the velocity form:

```
velocity += pull × (sum of the four neighbours' levels − 4 × level)     pull = (√(g × depth) × step ÷ spacing)²
velocity ×= keep                                                          damping, per sample
level    += velocity
level, velocity, foam ← what lay upstream a step ago                      where the water flows
foam     ← foam × exp(−step ÷ WATER_FOAM_SECONDS) + what breaking crests throw
```

- **Depth sets the speed.** A ripple on water `d` tiles deep travels at `√(WATER_GRAVITY × d)` tiles a second — 2.5 on a default pond, 0.6 across a puddle, 4.3 across a three-tile lake. The pull is capped under `MAX_COURANT_SQUARED`, clear of the scheme's limit.
- **The bottom drags shallow water.** `WATER_BOTTOM_FRICTION` over the depth (no shallower than `WATER_FRICTION_MIN_DEPTH`) is added to the damping, so a puddle stills in a moment where a lake rolls on.
- **The shore is where the field is dry.** A dry sample has `keep` 0, so it never moves, and a ripple that reaches it reflects. The dry ring round the field means every wet sample has four neighbours inside it.
- **What stands in the water is a shore too.** `resetWaterField` takes `WaterObstacle`s — the footprints of props standing in the water — and dries every sample under them, so ripples go round a crate as they go round a bank, and everything the shader does at a shore (foam, lapping, shelving) happens at its foot.
- **Beaches absorb.** Each wet sample's distance to the shore is measured once, when the field is shaped (a two-pass chamfer transform), and a sample within `WATER_BEACH_TILES` of it damps much harder (`WATER_BEACH_DAMPING`). Each dry sample's distance to the water — its **land** distance, from the water's edge, up to `WATER_WET_TILES` — is measured the same way, for the wet band.
- **Flowing water carries.** Where any sample flows, every step moves the level, the velocity and the foam downstream: each wet sample takes what lay upstream of it a step ago, read bilinearly (a semi-Lagrangian step). Standing water skips it.
- **Foam lingers.** A push throws foam in proportion to how deep it sinks the water past `WATER_FOAM_CALM_DEPTH` — so drizzle throws none, a wake leaves a trail and a blast a sheet — and so does a crest risen past `WATER_FOAM_CREST`. It thins by `1/e` every `WATER_FOAM_SECONDS`.
- **Ripples travel at the same speed at every resolution**; only the detail differs.
- **Drizzle.** `WATER_DRIZZLE_PER_TILE` small drops land a second on each tile of water, from the `WATER_RNG_STREAM` stream.
- **A long frame is cut short** at `WATER_MAX_STEPS_PER_FRAME` steps.
- **Flowing water has half the budget** (`WATER_MAX_FLOWING_SAMPLES`), since carrying it costs about as much again as the rest of a step (§7).

A push (`disturbWaterField`) lowers the water in a cosine bowl round a point, which rings out as it springs back. It touches only wet samples, and says whether it touched any.

### Why this is presentation

Nothing here is deterministic, and nothing needs to be: the field runs on the frame's clock, calls `exp`, `sqrt` and `cos`, and its drizzle comes from a stream no tick hashes. It reads the simulation's positions and cues after the ticks that made them, and nothing reads it back.

---

## 4. Drawing

### The surface and the band

`makeWaterSurfaceMesh` builds two things and lets `makeGroundMesh` shape both, so they have exactly the ground's rounded ends, filled bends and joined diagonals. First a **wet band**: every cell within one of the water, water included, its vertices carrying no water at all. Then the **water**: the cells of water themselves, each vertex carrying the water there. Both lie flat at `WATER_SURFACE_HEIGHT` — ten ground layer steps, over every layer the ground stacks and under the thickness of the built-in tile shape — and the band's triangles come first, so where the two overlap the water is drawn over the band. A fragment with no depth is the band's; that is how the shader tells them apart.

### The pass

The water is drawn once the scene's pass has ended, in the pass over it where the outline and the effects are drawn, and before them:

1. **The scene pass** draws the ground and every mesh, into the backbuffer and the scene's depth, as it always has.
2. **The copy.** `RenderedGameClient::recordSceneCapture` runs between the two passes, outside either, and the editor calls `WaterRenderer::captureScene` there: `RhiCommandList::copyTexture` copies the backbuffer into a texture of the renderer's own, as big as it and in `RhiDevice::backbufferFormat()`, made anew only when either changes.
3. **The water** is drawn over the backbuffer from that copy and the scene's depth, with its own builtin: `RhiDevice::tryCreateWaterPipeline`. No depth attachment: the water reads the depth instead, as the effects do, and is not drawn where something nearer than it stands in front. Premultiplied, though what it writes is opaque — it has already composed itself over the ground it read.

It reads three blocks: `WaterVertexUniforms` at vertex slot 1; `WaterShading` at fragment slot 0, which carries the scene's matrix too, so the shader can follow a ray and find where a point lands; and the scene's lights at fragment slot 1, as the very `MeshFragmentLights` block the mesh shader reads at its slot 0 — so the water is lit by the same list, the key light standing in when there is none, and cel-banded with the meshes. And four textures, `RHI_MAX_FRAGMENT_TEXTURES` of them, on every backend:

| Slot | Texture | Written |
|---|---|---|
| 0 | The field's motion (`writeWaterTexels`): slope along x and y ÷ `WATER_SLOPE_RANGE`, level ÷ `WATER_LEVEL_RANGE`, foam | Every frame, into the next of `WATER_FIELD_TEXTURE_COUNT` in turn |
| 1 | The copy of the scene | Every frame, by `captureScene` |
| 2 | The scene's depth | By the scene pass |
| 3 | What does not move (`writeWaterStillTexels`): shore distance ÷ `WATER_SHORE_TILES`, flow along x and y ÷ `WATER_MAX_FLOW_SPEED`, land distance ÷ `WATER_WET_TILES` | When the water is shaped anew (`setShape`) |

The field's textures are created with their first texels, which is what makes them CPU-writable on Metal.

### The shader

One body, shared word for word by all four backends: written in types every backend reads (`float2`, `float3`, `saturate`, `mix`), with a short prologue in each saying what those words mean there and how its textures and blocks are reached (`water_field_at`, `water_scene_at`, `water_scene_depth`, `water_screen_uv`, `water_clip_depth`), and an entry point that calls `water_shade`. Per fragment, in linear light:

| Term | What it does |
|---|---|
| Hidden | Where the scene's depth is nearer than the water's, nothing: something stands in front of it |
| Wet band | A band fragment: the ground the copy holds, raised to a power by how wet it is — darker, and richer, since the dim channels darken more — with a faint sheen of each light and of the sky, fading out `WATER_WET_TILES` from the water. Well inside the water, nothing |
| Lapping | Waves rolling in on every shore, at `WATER_LAP_WAVELENGTH` and `WATER_LAP_PERIOD`, arriving at different times along it: they tilt the surface within `WATER_LAP_REACH` of the bank, break into foam at it, and on the wet band run a glassy film with a line of foam at its lip up to `WATER_LAP_RUN` of the ground and draw it back |
| Depth | The vertex's depth, shelved by the still texels' shore distance (§2) |
| Colour by channel | How much of the ground the water lets through, per channel: `exp(−absorption × tint × depth)`, the absorption running from `clear_absorption` at opacity 0 to `murky_absorption` at opacity 1, evenly on a log scale, and `absorption_tint` making red go first and blue last — so shallow water keeps the ground's colour, the middle depths turn teal, and only the deep is all the water's own colour, darkened by up to `deep_darkening` over `colour_depth` tiles |
| Refraction | The ground is read from the copy a little way along the ripples' slope, further under deeper water (`refraction`, up to `WATER_BEND_DEPTH`) — unless what lies there stands in front of the water, when the ground straight under it is read instead |
| Ripples | The field's slope, drawn `WATER_RIPPLE_GAIN` times steeper than it is, so a wake a few hundredths of a tile high reads |
| Wind waves | Six directional waves travelling at deep water's speed for their wavelength; Low draws the two broad ones, High all six, Flat none. Peaked by `choppiness` — sharper crests and broader troughs than a sine's — their lines bent by a slow warp and their height by gusts, so a lake does not show the pattern repeating. Calmer where the water is shallow |
| Running water | Where it flows, the waves, caustics, foam and bubbles are read at two places the flow carried the surface from, half a `WATER_DRIFT_CYCLE` apart and cross-faded, so the surface runs without stretching; faster water draws foam out into streaks along its flow |
| Sky | Schlick's Fresnel term for water — 2% looking straight down, about 5% from the isometric camera's 30° — plus a term linear in how far a ripple's face is turned towards the eye (`WATER_RIPPLE_SKY`); dimmed as the scene's light is |
| Reflection | The reflected ray is followed through the scene's depth, `WATER_TRACE_STEPS` (24) steps over `reflection_reach` tiles (6, enough for the isometric camera to mirror the whole of a prop two tiles tall) and refined by halving, and whatever surface first stands in its way is mirrored — fading at the screen's edge and the end of its reach — by `reflection` × the Fresnel term plus `WATER_MIRROR`. From the fixed camera a reflection falls straight down the screen from what casts it, so only what stands in the water, or on the bank behind it, is mirrored in it |
| Caustics | Threads of light the waves focus on the ground, brightening it under the water; fainter at Low, none at Flat |
| Lights | Every light in the block: ambient plus a Lambert term per light on the rippled normal, falling off as the meshes' point lights do and flattened into the same bands, lighting the colour and the foam |
| Glints | Each light's highlight off the wave faces, in that light's colour: a narrow cone (`WATER_GLINT_SHARP`, 600), as a light's reflection off water is, so it scatters into sparkles where a wave face catches it rather than whitening the surface — from the isometric camera the default key light sits where a broad highlight would catch nearly every wave |
| Contact | Eight directions round each fragment are looked along on the water's own plane for a surface just above it — the foot of something standing in the water, a crate or a wading actor — and a shadow and a ring of foam are laid where one is near |
| Foam | A band along every shore, the lapping waves' breaking, the contact rings, the field's lingering foam — broken into bubbles that open up as it thins — and at High the crests a hard push raises |
| Filtering | A pixel's width in tiles (`texel.w`, the camera being orthographic) decides what detail is drawn: a wave, a ripple, a caustic thread or a bubble finer than a few pixels is left out rather than left to shimmer, and the slope it would have added widens and dims the glints instead |

The eye direction comes from the matrix's depth row, as the volumetric smoke's does ([fx.md §5](fx.md#5-volumetric-smoke)): the camera is orthographic and fixed, so it is the same for every pixel. A backend without the pipeline (the stub) draws no water.

### Splashes

`waterSplashEffect` is the spray a splash throws — droplets flung up and falling back as streaks, a lower ring of them thrown wide, and a wisp of mist, all lit — played through the effects (`playFxEffect`) pointing up and scaled to what landed. The editor plays it where shots and blasts land in water and every `EDITOR_WADE_SPLASH_TILES` a wader walks through it (§6).

---

## 5. Fidelity

| Fidelity | Simulated | Drawn |
|---|---|---|
| **Flat** | Nothing: shaped at `WATER_FLAT_SAMPLES_PER_TILE` (2) for its banks, never stepped, and nothing pushes it | A still surface: depth, colour by channel, the wet band, sky, reflections, lights, glints, contact rings and shore foam; no waves, no lapping, no caustics, and it does not run however it flows. It still splashes |
| **Low** | 4 samples a tile | The surface, with ripples, broad swell, lapping, running water and faint caustics; reflections followed two thirds as far |
| **High** (default) | 8 samples a tile | All of Low, with finer wind waves, full caustics, foam on the crests and reflections followed their whole reach |

Fidelity is a **graphics setting, not a property of the level**: the same water is laid at every fidelity, and a slow machine is slow in every project. The editor keeps it in the user's application data (§6).

### Effects

Four parts of drawing water can be switched off by themselves, at any fidelity (`WaterEffect`, `WaterEffects`, handed to the renderer in `DrawParams::effects`). Each one off reaches the shader as a strength of nothing, and the shader skips its work rather than drawing it unseen:

| Effect | Word | What goes when it is off | What is saved |
|---|---|---|---|
| Reflections | `reflections` | The scene mirrored in the water; the sky is still reflected | The reflected ray and its up to twenty-eight depth reads a fragment |
| Refraction | `refraction` | The ground under the water bending with its ripples | A projection and a depth read a fragment |
| Contact foam | `contact` | Foam rings and shadows at the foot of what stands in the water; props in the ripple field still have shore foam round them | Sixteen depth reads a fragment |
| Caustics | `caustics` | The light the waves focus on the ground | Only the caustics' own shading |

They are graphics settings too, beside the fidelity in the same file, and all on until the user switches one off.

---

## 6. In the Editor

- **Water and Dry cards.** General › ground holds the six terrains and Erase, then **Water** and **Dry**. The Tile tool's brush is a terrain, water or dry (`EditorGroundBrush`): water lays water over whatever the brush covers — only its depth where there was water already, keeping that body's colour, opacity and flow, and the brush's where it was dry — and dry takes water off, leaving the terrain under it.
- **Depth.** With water as the brush, `,` and `.` step the depth it lays through `EDITOR_WATER_DEPTHS` — Puddle (1/16 tile), Shallows (1/4), Pond (1), Lake (3), Deep (8) — and the status line names it. Painting over water at another depth re-deepens it where the brush passes.
- **Bodies of water.** With the Select tool, a click on water selects the whole body — every cell of water joined to it, whatever its depth (`connectedWaterCells`) — before the ground under it. Its panel has a **Depth** row listing the named depths (naming a depth set by number, or a mix); **Colour R**, **G**, **B** and **Opacity** as sliders; **Flow Direction**, in degrees anticlockwise from east; and **Flow Speed**, a slider from standing to `WATER_MAX_FLOW_SPEED`. Each gesture is one undoable edit, and the brush takes the body's look and flow for new water, so painting more of a river paints river. Delete dries the body.
- **Undo.** A water edit is a ground paint (`PAINT_GROUND`) whose `water` list holds each changed cell's water on both sides (`EditorWaterChange`); a brush stroke carries its terrain and its water in one edit.
- **Saving.** `layers.water` — its own bounds and a run list per byte — beside `layers.terrain`, written only when there is water. A file from before water flowed has no flow runs and reads as standing water. A level from before water was a layer, which painted it as the terrain `tile:water`, is read as water in the default colour at its old depth, with the sand the old stacking drew under water given to the cell ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)).
- **What stands in it.** Every placement that is not an actor and whose box rises through the water's surface (`placementWorldBounds`) is a `WaterObstacle`: the water is reshaped round it whenever one moves, so ripples go round it and foam rings its foot.
- **Footsteps.** A step on a cell of water sounds like water, whatever terrain is under it.
- **The setting.** View › Water: Flat / Low / High, checked on the current one, and Water Reflections, Water Refraction, Water Contact Foam and Water Caustics, each checked while it is drawn and switched by a click; kept in `graphics.json` in the user's application data as `{"water": "high", "water_effects": {"reflections": true, "refraction": true, "contact": true, "caustics": true}}`, an effect left out being on.
- **Reshaping.** `EditorWater` rebuilds the field, the surface and the still texels whenever the document's water layer, the fidelity, or what stands in the water differs from what it last shaped to.
- **Lights.** The water is drawn with the scene's light list and shading bands, the ones the meshes were drawn with that frame.
- **What pushes it.** While a level is played at Low or High, every player and actor that moves across water pushes a wake in proportion to how far it went (`EDITOR_WADE_*`), and pushes it along two arms trailing behind it, opening at the 19.5° a wake opens at (`EDITOR_WAKE_*`); a shot that lands in it splashes (`EDITOR_SPLASH_*`); a blast over it heaves it (`EDITOR_BLAST_*`).
- **What splashes.** At every fidelity, a shot landing in water throws a splash (`EDITOR_SHOT_SPLASH_SCALE`), a blast a bigger one, and a wader's feet a small one every stride through it; `EditorWater::takeSplashes` hands them to the effects.
- **The clock.** The water ages on the frame's time while editing and playing, and stops with a paused playtest.
- **Agents.** `paint_water` lays or dries a rectangle, in a depth, colour, opacity and flow; `get_ground`'s `water` rows show where it lies; `select` with `target` `water` picks a body, `set_property` sets its `color_r`, `color_g`, `color_b`, `opacity`, `flow_direction` and `flow_speed`, `set_water_depth` its depth, `delete` dries it, and `get_selection` reports all of them; `get_water` lists the named depths and what the water did last frame — how often it was pushed and splashed, and how many placements stand in it; `set_water_fidelity` and the three `set_water_*` commands set the fidelity, and `set_water_effects` and the four `toggle_water_*` commands the effects, which `get_water` reports under `effects` ([capabilities.md](../editor/capabilities.md)).

---

## 7. Budget

| Cost | Budget | How it is held |
|---|---|---|
| Stepping the field and packing its texels, a frame | **≤ 1.0 ms** median, at `WATER_MAX_SAMPLES`, and flowing at `WATER_MAX_FLOWING_SAMPLES` | Water's share of Engine §7's 3 ms for render submission. `WATER_MAX_SAMPLES` is 512 × 512 samples — a 62 × 62-tile pond at High; wider water is simulated at half the resolution, and again, until it fits. Flowing water carries its level, speed and foam downstream every step — about as much again as the rest of the step — so it has half that budget: a 43 × 43-tile river at High, and a wider one a step coarser. Depth costs nothing a frame: each sample's pull, damping and where its water comes from are worked out once, when the field is shaped, and only the samples that flow are carried. Texels are packed a row at a time, a word a texel. Flat steps nothing. Measured by the two `[perf]` cases in `test_water_field`, which `scripts/perf-gate.sh` runs: 0.34 ms standing and 0.45 ms flowing on the development machine |
| Upload | One RGBA8 texture of the field a frame, and one copy of the scene | 1 MB at the cap; the copy is a GPU-side blit |
| GPU | One indexed draw; per fragment of water, eight lights, six waves (twice where it flows), a reflection of up to twenty-eight depth reads, and sixteen contact reads | Only where there is water, and a band a cell round it |

---

## 8. Testing

| Test | What it proves |
|---|---|
| `test_water_layer` | Water laid on a cell reads back, a dry cell reads as none, drying clears every byte |
| `test_water_depth` | Depths in steps with zero dry; the shelf from nothing at a bank to full depth, further for deeper water; depth, colour and flow blending across the tile between two waters; a dry corner lowering depth without darkening colour |
| `test_water_fidelity` | Every fidelity's word names it back; Flat simulates nothing and shapes coarsest |
| `test_water_field` | The field covers the water and a dry ring; shore distance; each sample as deep as its cells, shelving to the bank; a ripple faster across a lake than a puddle, and a puddle stilling sooner; the budget coarsening then refusing, and a river coarsening before a lake as wide; pushes; bounded at any resolution; a long frame cut short; dry under what stands in it, with the shore round it; foam thrown by a hard push, lingering and thinning, and none by drizzle; flow slowed at the bank and carrying foam downstream. `[perf]`: the budget above, standing and flowing |
| `test_water_texels` | Still water packs level and unsloped, deep water reads deep and land dry, a hollow slopes up away from its middle; the still texels hold the shore, the flow and the land beside the water |
| `test_water_surface_mesh` | The water covers its cells alone, over every ground layer; a lone cell is a round pool; each vertex carries depth, opacity and colour, blended between cells; a wet band a cell wide is drawn first, carrying nothing |
| `test_water_renderer` | Against a fake device: buffers and textures, one indexed draw with its three blocks and four textures, the field placed where the water is, the viewport placed over the copy, fidelity's detail — Flat drawn still — the scene's lights handed through, textures written in turn, the scene copied into a texture of its size and format and made anew when that changes, nothing drawn without the copy or the depth, the still texels written on reshaping, and everything handed back |
| `test_gpu_water_renderer` | On real Metal, and on Vulkan through MoltenVK, over a grey "ground" copied as the editor copies it: open water shows and the scene's depth hides it; clear water shows the ground where opaque water hides it; deeper water hides more; red water is red; a push changes what is drawn; High, Low and Flat draw differently; a red light reddens the water; wet ground beside the water is darker; running water draws differently from standing water |
| `test_editor_water`, `test_editor_water_ops`, `test_editor_graphics_file`, `test_agent_water` | Reshaping and Flat's stillness; wading, its wake's arms, splashing and aging; reshaping round what stands in the water when it moves; splashes owed for hits and strides, at every fidelity; water as a layer and not a terrain; laying keeping a body's look; depth, look and flow set on water only, flow in degrees and as a fraction; undo; bodies; the Depth row; the water layer saved and read, flow included, a file without flow read as standing, and old levels' water migrated; footsteps on water; every agent tool and the three menu rows |

DX12 and OpenGL compile here but do not run; their shader text is checked with `glslangValidator` (GLSL 4.60, and HLSL through glslang's HLSL front end).

---

## 9. Not Yet

- **Reflections are the screen's.** What is not on screen, or is hidden behind something else on screen, is not mirrored: the sky stands in for it.
- **Contact is looked for, not known.** The foam ring round a wading actor comes from the scene's depth, so it rings anything whose foot stands in the water, and nothing out of sight.
- **No shadows on the water.**
- **Eight lights a draw**, the meshes' budget, until the clustered light list exists ([fx.md §6](fx.md#6-flashes)).
- **Nothing in the simulation reads water.** Wading does not slow anybody however deep it is, and a current carries nobody.
- **Flow is authored, not solved.** Water flows the way each body is told to; it does not run downhill, round obstacles or through narrows of its own accord.
