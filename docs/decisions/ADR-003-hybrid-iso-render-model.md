# ADR-003: Hybrid 3D geometry and billboarded sprites under one depth buffer

**Status:** Accepted
**Date:** 2026-08-22
**Scope:** Engine

## Context

An isometric renderer has to answer one question well: what is in front of what. The classic 2D answer is a painter's-algorithm sort over sprites, ordered by depth along the view axis. It is simple until it isn't — long walls, overlapping footprints, and tall objects produce sorting cycles that no total order resolves, and the standard fixes (splitting sprites, hand-authored sort hints, per-cell topological sorts) get expensive and fragile.

Simplish wants 3D meshes for terrain and structures — real geometry gives correct occlusion, cheap dynamic lighting, and instancing — and sprites for characters and effects, where 2D art is cheaper to produce and animate at the density a horde shooter needs. Those two have to coexist, and the naive approach of drawing all geometry then all sprites (or the reverse) is wrong in both directions: a character behind a wall must be occluded, and a character in front of the same wall must not be.

Compounding it: at 2,000 enemies and 20,000 projectiles, any per-object CPU sort is a budget problem before it is a correctness problem.

## Decision

Render 3D geometry and sprites **interleaved in a single depth-buffered pass**, with sprites participating in the depth buffer rather than being composited over it.

- The camera is fixed orthographic at 45° yaw and 2:1 dimetric pitch (26.565°). It translates and zooms; it never rotates, and the renderer is permitted to depend on that.
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
- Sprite art must be authored against the fixed projection angle. Changing the camera angle later invalidates the art library — this is a one-way door, and the projection choice is [an open question](../../REQUIREMENTS.md#8-open-questions) that should be closed before art production starts.
- Alpha-test cutout gives hard edges; soft edges need the later translucent pass, which does not depth-write and therefore can sort incorrectly against other translucent effects. Acceptable for glows and smoke, not for anything gameplay-relevant.
- Height-ramp metadata is required per sprite archetype, which the asset pipeline must produce and validate.

### Implications for Future Work

- Camera rotation is out of scope permanently. Code may assume a fixed view direction.
- The sprite pipeline needs per-archetype height ramps as first-class metadata ([Editor §8](../editor/REQUIREMENTS.md#8-asset-pipeline)).
- Golden-image tests must cover the sprite/geometry interleave specifically — a character behind, beside, and in front of a tall structure — on every backend.
- If sprites are pre-rendered from 3D models ([open question 2](../../REQUIREMENTS.md#8-open-questions)), the pre-render step can emit depth and normal data directly, which would make this path cheaper and more accurate. That argues in favour of pre-rendering.
