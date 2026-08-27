# ADR-003: Hybrid 3D geometry and billboarded sprites under one depth buffer

**Status:** Accepted (projection amended 2026-08-26)
**Date:** 2026-08-22
**Scope:** Engine

## Context

An isometric renderer has to answer one question well: what is in front of what. The classic 2D answer is a painter's-algorithm sort over sprites, ordered by depth along the view axis. It is simple until it isn't — long walls, overlapping footprints, and tall objects produce sorting cycles that no total order resolves, and the standard fixes (splitting sprites, hand-authored sort hints, per-cell topological sorts) get expensive and fragile.

Simplish wants 3D meshes for terrain and structures — real geometry gives correct occlusion, cheap dynamic lighting, and instancing — and sprites for characters and effects, where 2D art is cheaper to produce and animate at the density a horde shooter needs. Those two have to coexist, and the naive approach of drawing all geometry then all sprites (or the reverse) is wrong in both directions: a character behind a wall must be occluded, and a character in front of the same wall must not be.

Compounding it: at 2,000 enemies and 20,000 projectiles, any per-object CPU sort is a budget problem before it is a correctness problem.

## Decision

Render 3D geometry and sprites **interleaved in a single depth-buffered pass**, with sprites participating in the depth buffer rather than being composited over it.

- The camera is fixed orthographic at **zero yaw and 4:3 dimetric foreshortening**: world +X runs straight across the screen, world +Y is foreshortened to 3/4, and world +Z rises straight up the screen unforeshortened. Tiles are axis-aligned rectangles, not diamonds, and vertical surfaces are seen face-on. It translates and zooms; it never rotates, and the renderer is permitted to depend on that. (Amended 2026-08-26 — see [Amendment](#amendment-2026-08-26-straight-on-projection).)
- Terrain, structures, and large props are 3D meshes, instanced, depth-tested and depth-written normally.
- Characters, small props, and effects are camera-facing billboarded quads. Each writes per-pixel depth derived from its world footprint and a declared height ramp — the sprite's base sits at its world-space ground position, and depth increases up the sprite according to the ramp, so a tall sprite occludes correctly against geometry both in front of and behind it.
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

**What changed.** Yaw goes to zero and the height axis is drawn unforeshortened:

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
