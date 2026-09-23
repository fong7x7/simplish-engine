# Ground — Painted Terrain, Autotiled

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.2
**Packages:** `src/engine/render-ground/` (`eng`), and the editor's `editor-ground-*`, `editor-terrain*` and `simplish-editor-ground.cpp` in `src/editor/shell/`
**Governed by:** [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md) — terrain is mesh geometry in the depth-buffered pass
**Status:** First slice built and tested. The editor's Tile tool paints a level's floor cell by cell with a brush, the painted areas join each other with rounded edges and filled bends, and the result is saved in the level file's tile layer. The simulation does not read the ground yet: no terrain blocks or slows anybody (§6).

What a floor is painted with. A road is laid by painting its cells, and it rounds its own ends, bends with a curve and runs straight between; a patch of sand has soft corners and a road laid through it sits on it. Nobody picks edge or corner pieces: every shape is worked out from the cells around it.

---

## 1. The Shape

```
  GroundGrid: a terrain number per cell, 0 bare         ground-grid.h
       │
       ├─ groundQuarterShape(grid, layer, cell, quarter) ──► EMPTY / FULL / ROUND / FILLET
       │                                                     ground-quarter-shape.h
       ├─ makeGroundMesh(grid, layer_count) ──► flat layers, stacked, one atlas
       │        │                               ground-mesh.h
       │        ▼
       │   MeshInstance{mesh, identity, atlas} ──► MeshRenderer::draw (the scene pass)
       │
       └─ encodeGroundRuns / decodeGroundRuns ──► the level file's tile layer
                                                    ground-runs.h
```

| Piece | Header | What it owns |
|---|---|---|
| `GroundCell`, `GroundRect` | `engine/render-ground/ground-cell.h`, `ground-rect.h` | A cell by integer coordinates, and a rectangle of them |
| `GroundGrid` | `engine/render-ground/ground-grid.h` | A rectangle of terrain numbers that grows to hold whatever is painted |
| `GroundQuarter`, `groundQuarterShape` | `engine/render-ground/ground-quarter.h`, `ground-quarter-shape.h` | The autotiling rule |
| `makeGroundMesh` | `engine/render-ground/ground-mesh.h` | The geometry, and the atlas layout its texture coordinates assume |
| `GroundRun`, `encodeGroundRuns`, `decodeGroundRuns` | `engine/render-ground/ground-run.h`, `ground-runs.h` | Run-length coding, for the level file |

The engine knows terrain *numbers* and nothing else. Which terrain a number is, what colour it is and what it is called belong to the caller's palette — the editor's is `EDITOR_TERRAINS` (§5).

---

## 2. The Grid

A `GroundGrid` is a rectangle of `uint8_t`s, row by row from the south-west. Terrain 0 is bare ground and is what every cell outside the rectangle holds, so a grid starts empty and grows to cover what is painted without a level size being chosen first. It grows a block of `GROUND_GROWTH_BLOCK` (16) cells at a time, so a stroke dragged outward does not reallocate per cell, and it will not hold a cell past `GROUND_COORDINATE_LIMIT` (2048) from the origin on either axis — a level is a hundred-odd tiles across, and the limit is there so a mistyped coordinate cannot ask for gigabytes.

Coordinates are integers throughout. A cursor's world point is floored to a cell once, at the edge (`editorBrushRect`), and nothing after that does arithmetic on it in floats: a painted cell is the same cell on every machine, which is what the simulation will need when it reads the ground (§6).

---

## 3. Autotiling: Four Shapes a Quarter

Each cell is drawn a quarter at a time. A quarter reaches one corner of its cell, and its shape depends on its own cell and the three that meet it at that corner — the two beside it and the one across:

| The quarter's own cell | Beside it (either) | Across the corner | Shape |
|---|---|---|---|
| painted | painted | — | **FULL** — the whole quarter |
| painted | — | painted | **FULL** — two cells touching only at a corner join there |
| painted | neither | not painted | **ROUND** — a quarter disc about the cell's centre, radius half a tile |
| not painted | both | — | **FILLET** — the quarter less that disc: the inside of a bend, filled with a curve |
| not painted | not both | — | **EMPTY** |

What makes these four enough is one property: along every edge between two quarters, each side covers from the painted cell's centre to the edge's midpoint and no further. So any two shapes placed side by side meet without a seam, and there is no tile set to author and no pair of pieces that does not fit. What comes out:

- A cell painted alone is a disc. A straight run is a band a tile wide with rounded ends.
- A bend is rounded on the outside and filleted on the inside.
- A large area is its rectangle with its outer corners rounded off and its inner corners filled.
- A diagonal staircase of cells joins into one road — the across-the-corner row of the table — though it reads as a chain of beads rather than a smooth diagonal. A smoother diagonal needs a shape that looks two cells away, which this rule deliberately does not.

---

## 4. Layers, and the Mesh

Every terrain is its own flat layer, drawn `GROUND_LAYER_STEP` (1/512 of a tile) above the one numbered before it so two layers never fight for the depth buffer. A cell counts as painted for a layer when its terrain is **that layer or a later one**. That is what stacks the layers without gaps: a road laid through sand has sand filled in under it, so the road's rounded edges sit on sand rather than on bare ground, and where a road leaves a sand patch the sand flares out a little along it. The order of the palette is therefore the stacking order — a later terrain is drawn over an earlier one where they meet.

`makeGroundMesh` builds every layer into one mesh, in world coordinates, with every normal +Z:

- A cell whose four quarters are all FULL is one quad, not sixteen triangles — most of a painted area is interior.
- A quarter the next layer up covers FULL is not drawn at all, since nothing of it could show. FULL is monotone down the layers, so asking the next one up is enough.
- ROUND and FILLET are fans of `GROUND_ARC_SEGMENTS` (6) segments over a quarter turn.

Texture coordinates address an atlas one swatch wide and `layer_count` swatches tall, layer 1 at the top, each swatch `GROUND_SWATCH_TEXELS` (32) square. Each cell maps its own square onto its layer's swatch, inset half a texel so filtering never reads the swatch beside it; a swatch that tiles therefore draws as one continuous surface however the cells join. The whole ground is one draw call.

The eight layers together stay well under the 1/32-tile thickness of the editor's built-in Tile shape, so a tile laid on painted ground still sits on top of it.

---

## 5. In the Editor

- **Terrains.** `EDITOR_TERRAINS` (`editor-terrains.h`) is the built-in palette, in stacking order: grass, dirt, sand, water, stone, road, hole. Each has a name, the word a file and the agent API use for it, a colour and a grain. `makeEditorGroundAtlas` (`editor-ground-atlas.h`) turns them into the atlas: each swatch its colour, speckled by a hash of the texel's place, so it is the same on every run and tiles by construction.
- **Painting.** The browser's general › ground folder holds a card per terrain and an **Erase** card. Dropping one on the viewport takes it as the brush, switches to the Tile tool and paints one dab there. With the Tile tool out, a left drag paints with a square brush centred on the tile under the cursor (`editorBrushRect`), `[` and `]` shrink and grow it from 1 to 9 tiles, the viewport outlines the cells it covers, and the middle button still pans. The status line names the brush.
- **Undo.** A stroke is one edit however many cells it crossed: the ground is copied when the button goes down, painted directly while it is held, and diffed against the copy when it comes up (`diffEditorGround`). The action is the list of changed cells with their terrain on both sides (`EditorGroundChange`), which is its own inverse.
- **Drawing.** The editor rebuilds and re-uploads the ground mesh whenever the document's ground differs from the one it last built, so a stroke, an undo, an agent's fill and a level being opened are all drawn without any of them saying so.
- **Saving.** `bounds`, `tile_palette` and `layers.terrain` in the level file ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)).
- **Agents.** `get_ground` and `paint_ground` ([capabilities.md](../editor/capabilities.md)).
- **Footsteps.** Each terrain also names the surface a step on it sounds like — road and stone are both stone, a hole is bare ground — and a playtest hears steps on the painted cell under each walker ([audio.md §9.1](audio.md#91-footsteps)).

---

## 6. Not Yet

- **The simulation does not read the ground.** Nothing about a terrain stops, slows or hides anybody, and a hole is a black patch. Making one block wants a decision this slice does not take: a player's collision boxes also block shots and line of sight, and a hole should do neither. The navigation grid is the natural reader — it already works in cells.
- **Terrains are the editor's, not the project's.** A project cannot add one or give one a texture of its own; per-project tilesets, as [Editor §4.1](../editor/REQUIREMENTS.md#41-the-grid) describes, would replace `EDITOR_TERRAINS` with a data table and the atlas with images.
- **No height.** The format reserves a height layer; nothing writes it.
- **One brush shape.** Square, from 1 to 9 tiles. No flood fill or rectangle select.
