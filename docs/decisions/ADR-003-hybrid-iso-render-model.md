# ADR-003: Hybrid 3D geometry and billboarded sprites under one depth buffer

**Status:** Accepted (projection amended 2026-08-26, 2026-09-08, and 2026-09-09; characters amended 2026-09-10)
**Date:** 2026-08-22
**Scope:** Engine

## Context

An isometric renderer has to answer one question well: what is in front of what. The classic 2D answer is a painter's-algorithm sort over sprites, ordered by depth along the view axis. It is simple until it isn't — long walls, overlapping footprints, and tall objects produce sorting cycles that no total order resolves, and the standard fixes (splitting sprites, hand-authored sort hints, per-cell topological sorts) get expensive and fragile.

Simplish wants 3D meshes for terrain and structures — real geometry gives correct occlusion, cheap dynamic lighting, and instancing — and sprites for characters and effects, where 2D art is cheaper to produce and animate at the density a horde shooter needs. Those two have to coexist, and the naive approach of drawing all geometry then all sprites (or the reverse) is wrong in both directions: a character behind a wall must be occluded, and a character in front of the same wall must not be.

Compounding it: at 2,000 enemies and 20,000 projectiles, any per-object CPU sort is a budget problem before it is a correctness problem.

## Decision

Render 3D geometry and sprites **interleaved in a single depth-buffered pass**, with sprites participating in the depth buffer rather than being composited over it.

- The camera is fixed orthographic at **zero yaw and 4:3 dimetric foreshortening**: world +X runs straight across the screen, world +Y is foreshortened to 3/4, and world +Z rises straight up the screen unforeshortened. Tiles are axis-aligned rectangles, not diamonds, and vertical surfaces are seen face-on. It translates and zooms; it never rotates, and the renderer is permitted to depend on that. (Amended three times — see the [straight-on projection](#amendment-2026-08-26-straight-on-projection), [projection as a project setting](#amendment-2026-09-08-projection-as-a-project-setting), and [the projection is a rotation](#amendment-2026-09-09-the-projection-is-a-rotation-not-a-shear) amendments.)
- Terrain, structures, and large props are 3D meshes, instanced, depth-tested and depth-written normally.
- Characters, small props, and effects are camera-facing billboarded quads — the horde always, and every character but a handful of heroes and bosses, which may instead be skinned meshes (see the [2026-09-10 amendment](#amendment-2026-09-10-skinned-meshes-for-a-handful-of-characters)). Each billboard writes per-pixel depth derived from its world footprint and a declared height ramp — the sprite's base sits at its world-space ground position, and depth increases up the sprite according to the ramp, so a tall sprite occludes correctly against geometry both in front of and behind it.
- Alpha-test cutout gives hard sprite edges that depth-write correctly. Genuinely translucent effects draw in a later back-to-front pass with depth-test but no depth-write.
- No CPU depth sort of gameplay objects. Ordering falls out of the depth buffer; CPU-side batching is by atlas page and material, chosen for draw-call count rather than for correctness.

## Alternatives Considered

### Alternative A: Pure 2D with painter's-algorithm sorting

- **How it works:** Everything is a sprite; sort by depth key each frame and draw back to front.
- **Pros:** Conventional; no depth-buffer subtleties; artists control ordering directly.
- **Cons:** Sorting cycles are unsolvable in general and need authoring workarounds. Sorting thousands of objects per frame costs CPU the budget cannot spare. Dynamic lighting on sprites requires a normal-map path that approximates what real geometry gives for free. Occlusion by structures becomes a manual problem.

### Alternative B: Pure 3D with sprites banned

- **How it works:** Everything is a mesh, including characters.
- **Pros:** One pipeline, no depth trickery, correct by construction.
- **Cons:** Character art becomes a 3D modelling and skinning pipeline — much more expensive per archetype, for an aesthetic that gains little at this camera distance. Animating hundreds of skinned meshes at horde density is a far worse performance problem than drawing hundreds of sprite quads.

### Alternative C: Layered composition — geometry pass, then sprite pass

- **How it works:** Draw all geometry, then draw sprites over it using a depth test but a separate ordering scheme.
- **Pros:** Simple to implement; each pass is internally consistent.
- **Cons:** Wrong at exactly the moment it matters. A sprite is either always in front of geometry or always behind it, so a character cannot walk behind a pillar and out again. Every workaround reintroduces per-object sorting.

## Design Principle References

- **Principle 4: Simplicity Over Flexibility** — a fixed, non-rotating camera is a constraint accepted deliberately, and it is what makes stable sprite footprints and cell-stable culling possible.
- **Principle 2: Budgets Are Requirements** — moving ordering from the CPU to the depth buffer removes a per-object sort from the frame budget at exactly the scale where it would hurt most.
- **Principle 3: Data-Oriented Over Object-Oriented** — batching by atlas page and material, with no per-object ordering dependency, keeps submission a linear pass over dense arrays.

## Consequences

### Positive

- Correct occlusion between sprites and geometry in both directions, with no authoring workarounds.
- No CPU depth sort for gameplay objects; ordering is free.
- Real dynamic lighting and shadows on structures; sprites receive light through a normal-map path that stays consistent with the 3D lighting environment.
- Batching is chosen purely for draw-call efficiency, since correctness no longer constrains draw order.
- Projectiles become a single instanced draw per archetype from a GPU-resident buffer.

### Negative

- The per-pixel depth write for sprites is a real shader cost and disables early-Z for that draw. It must be measured on the reference GPU; if it proves too expensive, the fallback is a per-sprite depth bias with a documented and accepted failure mode for extreme height ratios.
- Sprite art must be authored against the fixed projection angle. Changing the camera angle later invalidates the art library — this is a one-way door, closed by the amendment below before art production started.
- Alpha-test cutout gives hard edges; soft edges need the later translucent pass, which does not depth-write and therefore can sort incorrectly against other translucent effects. Acceptable for glows and smoke, not for anything gameplay-relevant.
- Height-ramp metadata is required per sprite archetype, which the asset pipeline must produce and validate.

### Implications for Future Work

- Camera rotation is out of scope permanently. Code may assume a fixed view direction.
- The sprite pipeline needs per-archetype height ramps as first-class metadata ([Editor §8](../editor/REQUIREMENTS.md#8-asset-pipeline)).
- Golden-image tests must cover the sprite/geometry interleave specifically — a character behind, beside, and in front of a tall structure — on every backend.
- If sprites are pre-rendered from 3D models ([open question 2](../../REQUIREMENTS.md#8-open-questions)), the pre-render step can emit depth and normal data directly, which would make this path cheaper and more accurate. That argues in favour of pre-rendering.

---

## Amendment (2026-08-26): straight-on projection

The original decision inherited the conventional 45°-yaw isometric camera, which closed [open question 1](../../REQUIREMENTS.md#8-open-questions) by default rather than on purpose. It is now closed deliberately, the other way.

**What changed.** Yaw goes to zero and the height axis is drawn unforeshortened (the height scale in the table below is superseded by the 2026-09-09 amendment; the yaw and the tile footprint are not):

| Axis | Before (45° yaw, 2:1) | After (zero yaw, 4:3) |
|---|---|---|
| World +X | right and down | straight right, scale 1.0 |
| World +Y | left and down | straight down, scale 0.75 |
| World +Z | up, sheared by the yaw | straight up, scale 1.0 |
| Tile footprint | 64×32 diamond | 64×48 rectangle |

**Why.** The target reference is the Stardew Valley / 16-bit JRPG viewpoint rather than the Diablo/Age of Empires one. Two things follow from it:

- **Vertical surfaces are seen face-on.** With yaw at zero the camera looks straight down the world Y axis, so a wall, a character, or a tree presents its front to the camera instead of a corner. That is exactly what a billboarded sprite already draws, so the sprite and the geometry it stands against agree — the awkward case under 45° yaw, where a billboard faces the camera but the mesh beside it shows two receding faces, disappears.
- **X and Z share a scale; Y does not.** That unequal foreshortening is what makes the projection dimetric, and it means a sprite's on-screen height is its world height with no correction factor. Sprite sheets can be authored at their true pixel height.

**What it costs.** Tiles no longer tessellate into the diamond lattice that makes 45° depth sorting a simple `x + y` ordering; depth along the view axis is now world Y alone, which is simpler still. The 3/4 ratio keeps clean pixel math at the 64 px tile size (64×48), but it is not the classic 2:1, so any tile art that assumed a 64×32 diamond must be re-authored. No such art exists yet, which is why this amendment lands now rather than later.

**Where it lives.** `src/editor/shell/include/editor/shell/iso-projection.h` — `ISO_TILE_WIDTH`, `ISO_TILE_DEPTH`, and `ISO_TILE_RISE`. Everything else about this ADR — the single depth-buffered pass, billboards writing per-pixel depth, no CPU sort — is unaffected.

> Superseded in part by the 2026-09-08 amendment below: these three constants are now the `ISO_AXES_DIMETRIC` half of a two-valued setting, and the numbers in the table above are what a project gets by default rather than what it is stuck with.

---

## Amendment (2026-09-08): projection as a project setting

The 2026-08-26 amendment called the projection a one-way door and closed it on the dimetric side. That call is reversed: the door is a project's to open, and both projections are supported.

**What changed.** The projection is a field in `project.json`, chosen per project and switched from **View › Dimetric View / Isometric View**:

| | Dimetric (default) | Isometric |
|---|---|---|
| Yaw | zero | 45° |
| World +X | straight right, scale 1.0 | right and down, 32 / 16 px |
| World +Y | straight down, scale 0.75 | left and down, 32 / 16 px |
| World +Z | straight up, scale 1.0 | straight up, scale 1.0 |
| Tile footprint | 64×48 rectangle | 64×32 diamond |
| Projection ray | 4 along Y per 3 up Z | 2 along each ground axis per 1 up Z |

A project that names no projection reads as dimetric, which is every project that exists today and is the projection they were authored against.

**Why.** The reference viewpoint is a per-project artistic choice, not an engine-wide one — a 16-bit JRPG project and a Diablo-style one want different lattices, and the renderer's dependence on the projection is on *a* fixed oblique projection, not on any particular one. Nothing in the depth model needed the specific numbers: the direction points collapse along is derived from the axes (`isoProjectionRay`), and the depth row of the view matrix is derived from that. Two projections cost one struct of five floats where the alternative was a second copy of every routine that draws, picks, or measures.

**What it costs.**

- **Tile art is still authored against one projection.** The setting is a project's, not a viewer's: switching it rotates the world under whatever art the project already has, which is why the switch writes itself into the project rather than into an editor preference. What was a one-way door for the engine is now a one-way door per project, taken knowingly at the point a project is started.
- **Generated thumbnails are cached per projection**, since a card shows an asset at the angle the viewport will show it at. Switching retires the cache for the projection being left, and switching back finds it again.
- **Free rotation is still out of scope.** Two fixed yaws are not a rotating camera: the renderer may still assume the projection is constant for the frame, and for the project.

**Where it lives.** `src/editor/project/include/editor/project/project-projection.h` holds the setting and its names; `src/editor/shell/include/editor/shell/iso-axes.h` holds the five numbers each projection maps the world axes with. `ISO_TILE_WIDTH` is still shared: a tile is 64 px across in both, so a tileset's pixel budget does not depend on the choice.

---

## Amendment (2026-09-09): the projection is a rotation, not a shear

Reported as a bug: a sphere dropped into the viewport drew as an oval, a quarter taller than it was wide.

**What was wrong.** A projection is a 2×3 matrix — one row taking a world point to screen X, one to screen Y. It preserves shape exactly when those two rows are perpendicular and the *same length*. The 2026-08-26 amendment chose the height scale for a property it wanted (a wall's on-screen height equals its world height, so sprite sheets could be authored at true pixel size) rather than deriving it, which made the rows different lengths: 64 across against 80 down. That is a shear, not a view — an oblique projection in the technical-drawing sense, where spheres are ellipses by construction.

Nothing looked broken, because everything the editor drew was built from the same axes: tiles, grid, cubes, and bounding boxes all agreed with each other. Only a round object could expose it, and the first one to be placed did.

| | Was | Now | Cause |
|---|---|---|---|
| Dimetric height scale | 64 | 42.332 = √(64² − 48²) | 48.6° camera pitch |
| Isometric height scale | 64 | 39.192 = √(2048 − 512) | 30° camera pitch, which is what a 2:1 diamond means |
| Sphere silhouette, dimetric | 64 × 80 | 64 × 64 | |
| Sphere silhouette, isometric | 46 × 68 | 46 × 45 | |

**What changed.** The height scale is no longer a number anyone picks. `isoRiseFor` in `iso-axes.h` derives it from the ground axes, and the ground axes are what a project's tile art is drawn against. Both tile lattices are untouched — 64×48 rectangles and 64×32 diamonds — so no art dimension moves. What moves is height: a one-tile cube is now 42 px tall in the dimetric projection rather than 64.

**What it costs.** The property the 2026-08-26 amendment was buying is gone: a sprite's on-screen height is its world height times cos(pitch), not times one. Sprite sheets need that factor, which is one constant in the sprite pipeline that does not exist yet. That is the whole cost, and it buys back every round or organic shape looking like itself.

The alternative was to keep the height scale and widen the tile to 80 px to match, which also squares the rows. It was rejected because it moves the tile lattice — the one dimension art is actually authored against — to fix a defect in the height axis.

**A note on the names.** Once the projection is a real rotation, the axis scales make "dimetric" the wrong word for the zero-yaw one: X, Y, and Z are foreshortened by 1, 0.75, and 0.661, all different, which is *trimetric*. The 2:1 one, where the two ground axes share a scale, is the one that is strictly dimetric. The names are kept as they are — they are the words games use for these two looks, they name the setting in `project.json` and the rows in the View menu, and renaming them would migrate a file format to win an argument about vocabulary.

**Where it lives.** `isoRiseFor` and `isoSqrt` in `src/editor/shell/include/editor/shell/iso-axes.h`. `test_iso_projection.cpp` asserts the row-length invariant directly; `test_editor_mesh_capture.cpp` renders a sphere through the CPU rasterizer and measures its silhouette, which is the assertion that would have caught this in the first place.

---

## Amendment (2026-09-10): skinned meshes for a handful of characters

Requested as a feature: rigged skeletons and animation. Alternative B above rejected skinning characters, and the reason it gave still holds for the horde. What is admitted here is narrower than what it rejected.

**What changed.** A small number of characters — the one to four players, a boss, a set-piece creature — may be drawn as **skinned meshes**: a rigged model, posed every frame by an animation clip, drawn in the same depth-buffered pass as the static geometry and the sprites. Horde enemies stay billboarded sprites, as the Decision says.

**Why.** Alternative B's cost argument scales with the number of characters. At horde density, skinning and drawing each enemy is the problem it describes; at four players and a boss it is a handful of draw calls. Those few are also where animation fidelity reads — a player aiming, reloading, turning; a boss telegraphing — and where 2D art is at its most expensive, since a player character needs every facing of every action. Nothing about the depth model changes: a skinned mesh writes depth like any other mesh, so a character behind a pillar is hidden by it and a sprite enemy in front of a boss is drawn over it, with no new ordering rule.

It also serves [open question 2](../../REQUIREMENTS.md#8-open-questions). A rig the engine can pose is the rig an offline step would pose to render sprite sheets, so the same loader and the same clips feed either answer.

**The limit, as a budget.** Skinned meshes are drawn one call per instance with a per-draw joint palette, deliberately without instancing: at the counts admitted, instancing would be machinery with nothing to pay for. The design point is **16 skinned instances per frame**, recorded in [Engine §7](../engine/REQUIREMENTS.md#scale-and-capacity). A design that wants more — a crowd, a swarm — wants sprites, and that is the answer rather than a reason to raise the number. One skin moves at most **80 joints** (`MESH_MAX_SKIN_JOINTS`), set by the smallest per-draw constant space any backend has — Metal's 4 KB of inline bytes; a full humanoid with finger bones is about 65.

**Determinism.** Animation is presentation, and the simulation never reads a posed joint ([ADR-002](ADR-002-fixed-timestep-determinism.md)). That is what lets clip time run off the render frame's delta, and slerp call `acos` and `sin`, without touching the determinism contract. Anything gameplay needs from an animation — where a muzzle is, how big a hitbox is this frame — must come from simulation data, never from a pose. If that ever has to change, posing moves into the tick under the full contract (a tick-driven clock, no libm transcendentals) and this amendment is revisited first.

**What it costs.**

- A second vertex path in every backend with a mesh pipeline: `RhiDevice::tryCreateSkinnedMeshPipeline`, with MSL, HLSL and GLSL skinning stages in front of each backend's existing mesh fragment shader. Vulkan has neither pipeline yet, so it draws neither static nor skinned meshes.
- A glTF 2.0 loader in-tree (`engine/gltf`), written over the nlohmann_json the engine already links rather than adding a dependency. It reads the subset a skinned character needs and refuses the rest by name.
- Character art for heroes becomes a modelling and rigging pipeline, which is the cost Alternative B named. It is accepted for the few characters that justify it, and nowhere else.

**Where it lives.** `src/engine/animation/` holds skeletons, skins, clips, and pose sampling — pure math, no GPU. `src/engine/gltf/` reads rigged `.gltf` and `.glb` files. `src/engine/render-mesh/` gains the skinned vertex, `SkinPalette`, and `SkinnedMeshRenderer`. [animation.md](../engine/animation.md) is the technical write-up.

