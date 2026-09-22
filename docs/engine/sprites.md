# Sprites — Billboards Cut Out of a Sheet

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §5.2
**Packages:** `src/engine/render-sprite/` (`eng`), and the editor's `editor-sprite-*` in `src/editor/shell/`
**Governed by:** [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md), and the [2026-09-22 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-22-a-billboard-is-an-upright-quad-not-a-depth-ramp) that settled what a billboard is
**Status:** First slice built and tested. A sprite billboard is dragged into a level from the editor's browser, pointed at one of the project's sprite sheets, cut into a grid and played; it is drawn in the same depth-buffered pass the meshes are, so what is in front of it hides it and what is behind it does not. Atlas packing, eight-direction facing, and the horde's own sprite path are still ahead (§7).

The sprite half of the hybrid renderer, begun from the editor end: one billboard a designer can place and see, rather than the batcher a horde will need.

---

## 1. The Shape

```
  a sheet image under the project's assets                (assets/sprites/slime.png)
            │  scanned by extension          editor-asset-scan.h
            ▼
  EditorSprite: a sheet path, a grid, a speed, a height   editor-sprite.h
            │
            ├─ spriteFrameAt(grid, seconds) ──► which frame the render clock is on
            │
            ├─ spriteFrameUv(grid, frame) ──► insetSpriteUv ──► makeSpriteQuadMesh
            │                                    one quad per frame, uploaded once
            │
            ├─ editorSpriteWidth(axes, sprite, frame pixels) ──► its world width
            │
            └─ makeSpriteTransform(axes, sprite, width) ──► the model matrix
                        │
                        ▼
              MeshInstance{quad, model, sheet texture} ──► MeshRenderer::draw
                        │                                   (the scene pass, with the meshes)
                        └─ the fragment stage discards texels below MESH_ALPHA_CUTOFF
```

| Piece | Header | What it owns |
|---|---|---|
| `SpriteSheet` | `engine/render-sprite/sprite-sheet.h` | The grid a sheet is cut into, and the speed its frames play at |
| `SpriteUvRect` | `engine/render-sprite/sprite-uv-rect.h` | The part of a sheet one frame covers |
| `spriteSheetFrameCount`, `spriteFrameAt`, `spriteFrameUv`, `spriteFramePixels`, `insetSpriteUv` | `engine/render-sprite/sprite-sheet-frames.h` | Which frame is showing, where it sits, and how big it is |
| `makeSpriteQuadMesh` | `engine/render-sprite/sprite-quad.h` | The four vertices one frame is drawn on |
| `MESH_ALPHA_CUTOFF` | `engine/render-mesh/mesh-alpha-cutoff.h` | The alpha a texel has to clear to be drawn at all |
| `EditorSprite` | `editor/shell/editor-sprite.h` | One billboard placed in a level |
| `editorSpriteWidth`, `makeSpriteTransform` | `editor/shell/editor-sprite-transform.h` | How wide it stands, and how it is turned to face the camera |
| `EditorSpriteQuadKey` | `editor/shell/editor-sprite-quad-key.h` | What decides a quad, and what the editor keeps them under |
| `EditorSpriteSheetTexture` | `editor/shell/editor-sprite-sheet-texture.h` | A sheet image uploaded once, shared by every billboard on it |

---

## 2. Why a Billboard Is Upright

ADR-003 called for sprites that write per-pixel depth from a declared height ramp. The [2026-09-22 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-22-a-billboard-is-an-upright-quad-not-a-depth-ramp) replaces that with a quad standing upright in the world, because the ramp is what a *screen-aligned* quad needs and an upright one does not:

- The projection is oblique, so a point higher up an upright quad is further along the view ray than its base. The depth the ramp would have declared is the depth the geometry has.
- It is therefore an ordinary opaque mesh: depth-tested, depth-written, resolved per pixel against meshes and against other billboards with no sort and no ordering rule.
- Nothing writes depth from the fragment stage, so early-Z stays on — the cost ADR-003 listed as this decision's main negative.

Its facings come from the projection rather than from the sprite: `isoScreenRight` gives the world direction drawn level across the screen, world +Z is drawn straight up, and `isoProjectionRay` gives the direction points collapse along. The quad's local X goes along the first, its local Z along the second, and its local +Y — the axis no vertex sits on, which carries only the normal — along the third, so it is shaded as a surface facing the camera without being leaned out of the upright. A camera that rotated freely would break this; two fixed yaws do not, which is the constraint ADR-003 takes deliberately.

---

## 3. The Cutout

A sprite's empty corners must not draw. The scene pass is opaque and depth-writing, so the only way for them not to is for their fragments not to exist: each backend's mesh fragment stage discards a texel whose alpha is below `MESH_ALPHA_CUTOFF`, a half.

That is five lines in one shader per backend — Metal's MSL, the two GLSL copies, and the HLSL — rather than a pipeline of its own. Opaque geometry never reaches the branch, because an image with no alpha channel loads with every texel at 1, as does the renderer's untextured stand-in. The number is spelled out again in each shader, since none of them can include a C++ header; `mesh-alpha-cutoff.h` says so and is the one place to change it.

`test_gpu_sprite_cutout.cpp` draws a billboard with a map that is opaque down one half and empty down the other, on whichever backend the build selected, and reads the target back: the empty half must be the colour the pass cleared to.

---

## 4. Sizing: The Factor the Projection Needs

The [2026-09-09 amendment](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-09-the-projection-is-a-rotation-not-a-shear) made the projection a rotation rather than a shear, and left a loose end: "a sprite's on-screen height is its world height times cos(pitch), not times one. Sprite sheets need that factor, which is one constant in the sprite pipeline that does not exist yet."

This is where it lands. A world unit covers `isoAcrossPixels(axes)` pixels along the screen's horizontal and `IsoAxes::z_up` pixels up it, and those two are not the same number. So only the height is authored, and the width is derived:

```
width_tiles = height_tiles × (frame_px_width / frame_px_height) × (z_up / isoAcrossPixels)
```

Two things follow, and both are asserted in `test_editor_sprite_capture.cpp` by rasterizing a billboard and measuring its silhouette:

- A square frame draws square, under either projection.
- A billboard one tile tall is drawn exactly as tall as a one-tile cube beside it — 42 px dimetric, 39 px isometric, at zoom 1.

There is no width row in the properties panel, and no width key in a level file. A row that could stretch a sprite is a row that will.

---

## 5. Frames, and the Quads They Are Drawn On

A sheet is a grid — columns, rows, and how many of the cells hold a frame — plus a speed. The three are separate because they disagree often: a 4×3 sheet of ten frames has two empty cells, and playing them would blink the sprite out twice a loop.

`spriteFrameAt` takes the frame index from the render clock through a modulus rather than by accumulating an index, so a clock that has been running for an hour lands on the frame its own time says. Frame time is presentation time ([ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md)): nothing in a tick reads a billboard, and a level's billboards are never hashed.

One frame's texture coordinates are baked into a four-vertex quad rather than passed to the shader, since the mesh pipeline has no per-instance UV transform. The coordinates are pulled in half a texel on every side: sampling is bilinear, and a coordinate exactly on a cell seam mixes the neighbour into the frame's edge.

The editor keeps those quads under an `EditorSpriteQuadKey` — the grid, the frame, and the sheet's pixel size, which is what the inset is measured from. Two billboards on the same sheet cut the same way therefore share every quad, and a sheet playing at twelve frames a second uploads nothing after its first loop. Re-cutting a grid in the panel retires the quads it had; past a bound the whole cache is released and refilled with the frames actually being shown.

---

## 6. What a Level Holds

A billboard is saved as an entity of definition `entity:sprite_billboard` ([project-format.md §4.1](../editor/project-format.md#41-what-the-editor-writes-today)): the sheet it shows as a path under the project's assets, where its base stands, how tall it is, and the grid and speed it plays at.

The path rather than an index: sheets are not placeable assets and carry no index, a path survives a rescan, and it is what reads in a hand-edited file. A sheet the project no longer holds is kept as written and the Sheet row says `(missing)`, rather than the billboard being silently repointed at whatever is first.

---

## 7. What Is Not Here

- **No atlas, and no batcher.** One draw per billboard, which is right for the handful a level is authored with and wrong for a horde. §5.2's batching by atlas page is what the horde will need, and it needs the offline packer too (§4.1's asset pipeline).
- **No facings.** A sheet is one row of animation, not eight directions of it. `EditorSprite` names a grid and a frame range; which row an actor's facing picks is a runtime question nothing asks yet.
- **No sprites in the simulation.** A billboard is scenery: it stops nobody, and the game does not spawn one. What draws an actor in a playtest is still its model.
- **One ramp, and it is linear.** See the amendment: a sprite wanting its own needs the fragment-stage depth write ADR-003 describes, and nothing here forecloses it.
