# Spatial — Navigation Grid, Line of Sight, Paths

**Parent document:** [Engine REQUIREMENTS](REQUIREMENTS.md) §6
**Package:** `src/engine/spatial/` (`eng::spatial`), plus `sinCosDegrees` and `turnToward` in `src/engine/math/`
**Governed by:** [ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md); consumed by the actors of [ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md)
**Status:** First slice built and tested: the navigation grid, line of sight, A*, path smoothing, and reachability. Driven by `src/game/actors` ([actors.md](../game/actors.md)) and drawn by the editor's Navigation Overlay. Flow fields, the spatial hash and a static-geometry broadphase are not written — §6.

Where characters of a given size can stand, whether one can see or walk straight to a point, and the shortest way there when it cannot. Game-agnostic: nothing here knows what an actor or a behavior is. Everything here runs inside the tick, so all of it is under the determinism contract.

---

## 1. The Shape

```
physics::CollisionBox list ──► NavGrid (clearance per cell)
                                   │
          hasLineOfSight ◄─────────┤  sight: clearance ≥ 1   walking: ≥ requiredClearance(r)
          nearestOpenCell ◄────────┤  a goal nobody can stand on → the nearest spot someone can
          PathFinder::find ◄───────┘  A*, integer costs, fixed tie-breaks, expansion budget
                 │
          smoothPath ──► a few straight legs (≤ 16 waypoints)
```

| Piece | Header | What it owns |
|---|---|---|
| `NavGrid` | `engine/spatial/nav-grid.h` | Cells over a rectangle of floor; each cell's clearance |
| `NavGridSpec`, `fitNavGrid` | `nav-grid-spec.h`, `nav-grid-fit.h` | Where a grid lies and how fine it is; fitting one round a level |
| `hasLineOfSight` | `line-of-sight.h` | Whether a segment crosses only cells with enough clearance |
| `nearestOpenCell` | `nearest-open-cell.h` | The nearest cell a character of a given size can stand in |
| `PathFinder`, `PathRequest`, `PathResult`, `PathStatus` | `path-finder.h` … | A* over the grid, with scratch memory reused across searches |
| `smoothPath` | `path-smoothing.h` | Cells to waypoints: the furthest cell a straight walk reaches, repeatedly |
| `canGridStep`, `GRID_STEPS` | `grid-step.h` | The eight steps between neighbouring cells, and whether one is open — the one rule A* and reachability share |
| `reachableCells` | `reachability.h` | Every cell a character of a given size can walk to from any of a set of cells |
| `sinCosDegrees`, `turnToward` | `engine/math/sin-cos.h`, `turn-toward.h` | Deterministic sine and cosine; turning a heading by at most a fixed angle |

`sinCosDegrees` lives in `math` because [Engine §3](REQUIREMENTS.md#3-platform--tech-stack) promises simulation code deterministic replacements for libm's transcendentals, and this is the first of them; nothing about it is spatial.

---

## 2. The Grid

A `NavGrid` covers a rectangle of floor in square cells a **quarter tile** across (`NAV_CELL_SIZE_TILES`). A quarter rather than a half, because a player is 0.3 tiles in radius and walks through a one-tile gap between two props: at half-tile cells that gap has no cell far enough from both props to stand in, and a planner would call a corridor a wall.

**Built from the boxes collision uses.** The grid rasterizes the same `physics::CollisionBox` list the tick resolves characters against, so what a planner routes round is exactly what movement stops against. A box blocks a cell when it overlaps it within the height band the spec names — from the floor to `NAV_CLEAR_HEIGHT_TILES` above it — so a sign overhead and a decal underfoot block nothing. Rasterizing is conservative: a box edge anywhere inside a cell makes the whole cell solid, and an edge exactly on a cell boundary does not reach into the next.

**Clearance, not walkable bits.** Each cell holds its chessboard distance to the nearest solid cell, in a byte — 0 for a solid cell, saturating at 255 — computed by two sweeps of the grid. One number serves characters of every size: a character of radius *r* fits in a cell when its clearance is at least `requiredClearance(r) = ⌈r / cell + ½⌉`, which is what guarantees the nearest solid cell's nearest edge is at least *r* from the cell's centre. Sight asks for clearance 1, any cell not itself solid, so a character sees through a gap it cannot fit through.

**Fitted, not authored.** There is no tile layer yet, and nothing says how big a level is, so `fitNavGrid` takes the rectangle holding every box and every spawn, adds `NAV_GRID_MARGIN_TILES` (8) on every side, snaps the origin down to a whole tile — so tile-aligned props fill whole cells and moving one prop does not shift the grid by a fraction of a cell — and caps each side at 1,024 cells. The grid is derived data and is never saved ([project-format.md §11](../editor/project-format.md#11-deliberately-not-in-the-format)); it is a pure function of the setup, so every peer builds the same one.

---

## 3. Line of Sight

`hasLineOfSight(grid, from, to, clearance)` walks exactly the cells the segment crosses, in order, stepping to whichever cell boundary it meets first (Amanatides–Woo). Where the segment passes exactly through a corner, both cells beside the corner must pass too, so a line cannot slip between two solid cells touching diagonally — the same rule A*'s diagonal steps follow. Cells off the grid pass: the grid covers everything solid with a margin, so off it there is nothing in the way, and a grid with no cells blocks nothing.

The walk's step count is bounded by the cells between its ends; a rounding that loses the end cell returns false rather than walking on.

---

## 4. Paths

`PathFinder::find(grid, request)` is A* from one cell to another for a given clearance, with an expansion budget.

| Rule | Why |
|---|---|
| Costs are integers: 10 along an axis, 14 diagonally, with the matching octile heuristic | No float ever decides an ordering, and 14 keeps the heuristic consistent, so no cell closes twice |
| Ties go to the smaller estimated remainder, then the smaller row-major index; neighbours are visited in a fixed order | Two equal paths resolve the same way on every machine |
| A diagonal step needs both cells beside it open | A character never clips the corner it rounds |
| Scratch is sized once and stamped per search rather than cleared | No allocation during a tick; starting a search costs nothing in the grid's size |
| `max_expansions` bounds a search; `OVER_BUDGET` says it was cut short | One request cannot blow a tick; the caller decides whether to try again |

`PathResult` carries a status — `FOUND`, `UNREACHABLE`, `OVER_BUDGET`, or `BLOCKED_ENDPOINT` for a start or goal off the grid or too narrow — the number of cells expanded, the cost, and the path as a view of the finder's own buffer, valid until it searches again.

A path of cells steps in eight directions and zigzags. `smoothPath` turns it into straight legs: from each waypoint, the next is the furthest cell along the path a straight walk with the character's clearance still reaches. Scanning stops at the first cell that fails, so smoothing is linear in the path's length. The output is a fixed-size span; a caller whose buffer fills walks what it has and plans again from there.

`reachableCells(grid, from, clearance)` floods out from a set of cells by the same eight steps and the same corner rule as A*, and marks every cell it reaches. It answers "can anything get from here to there at all" for every cell at once, where A* answers it for one pair: the editor floods from the player starts to find floor walled off from them, and from each actor to find actors that can never reach a start. It allocates, and is for the editor, not the tick.

When a goal is somewhere a character cannot stand — a player backed against a crate, a random spot on top of a prop — `nearestOpenCell` searches rings outward by chessboard distance and takes the nearest open cell in the first ring holding one, ties to row-major order.

---

## 5. Determinism, As Implemented

| Requirement ([§4.3](REQUIREMENTS.md#43-determinism-contract)) | Where it is kept |
|---|---|
| No libm transcendentals | `sinCosDegrees` reduces whole quadrants in degrees with `fmod` and `nearbyint`, then evaluates Taylor series to r¹³ and r¹⁴ in double — only operations IEEE-754 rounds exactly, with FMA contraction off. 90° gives exactly 1 and +0. `test_sin_cos` pins golden bits |
| Ordered iteration | Rasterizing visits boxes in setup order; both clearance sweeps are fixed raster order; A*'s heap order is total |
| Integer where discrete | Path costs, clearance, cell coordinates |
| No allocation in a tick | `PathFinder` scratch and the smoothing buffer are sized outside the tick |

Nothing here is hashed: the grid is a pure function of the setup, and a path lives in the caller's state, which the caller hashes.

---

## 6. Not Yet

| Gap | Waiting on |
|---|---|
| **Flow fields** — one integration field per player, sampled by every pursuer | The horde (Game §5.2); `pursue` switches to them behind the same action |
| **Spatial hash** for neighbour queries | Actors separating from many neighbours, and actors targeting actors |
| **Static-geometry broadphase** in front of `resolveCylinderAgainstBoxes` | Levels with more props than every-box-every-tick can afford |
| **Incremental updates** — a door opening mid-run | Logic's `open_door` ([project-format.md §7](../editor/project-format.md#7-logic-and-expressions)); the grid then becomes state and is hashed |
| **Tile-layer bounds** in place of `fitNavGrid` | The level format's tile layers |
| **Multiple floors** | [Open question 3](../../REQUIREMENTS.md#8-open-questions): the grid is one flat floor |
