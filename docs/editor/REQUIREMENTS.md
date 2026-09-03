# Simplish — Editor Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.2
**Status:** First slice implemented — project loading, toolbar, viewport
**Last Updated:** 2026-08-22

---

## 1. Overview

The editor is a desktop application that links `src/engine/` and `src/game/` and authors the content those layers consume: levels, encounters, waves, and the data tables behind weapons and enemies. It renders through the same isometric pipeline the game uses and simulates through the same deterministic tick, so what a designer sees in the editor is what ships.

**Desktop only** (macOS, Windows, Linux). Never built for consoles.

### Current State

A first slice builds and runs: `./build/debug/src/bin/editor/simplish-editor [project-directory]`.

| Package | Covers | State |
|---|---|---|
| `src/editor/project/` | The `.simplish/project.json` format, open and create, `last_opened_at` stamping, the recent-projects list | Built; 22 tests |
| `src/editor/shell/` | Title bar, menu bar (File / Edit / View / Help), tool toolbar (Select / Tile / Height / Prop / Entity), the dimetric viewport with left- or middle-drag pan and scroll zoom, and the asset strip that drags models into the world | Built; 106 tests |
| `src/bin/editor/` | Entry point: resolves the data directory, opens a project given on the command line | Built |

What the slice deliberately does not do yet: nothing is authored. The tools select but do not edit, the viewport draws a grid but holds no level data, and there is no dockspace, save, or undo. Those arrive with the level format (§4.4) and the sections below.

Asset placement is the second exception. The strip along the bottom lists the `.obj` files under `<project>/assets/`, and dragging one onto the viewport loads it, uploads it, and draws it as a real depth-tested mesh on the tile it was dropped on. What it does not do is persist: placements live in memory until the level format (§4.4) gives them somewhere to go, so closing the editor loses them. It is a placement tool, not yet an authoring one.

The menu bar is the exception that proves the point. It is built — File, Edit, View, and Help, with dropdowns, separators, accelerator hints, and recent projects — but most of what a menu bar traditionally offers has nothing behind it yet. Rather than hide those commands, the bar lists them disabled, so the menu reads as the shape of the editor rather than only the parts that happen to exist.

Two decisions the slice locks in:

- **A dropped model is scaled to one tile and stood on the ground.** Models arrive in whatever unit their author used, and a metre-scale crate beside a centimetre-scale one is unreadable. Scaling by the larger horizontal extent keeps each model's own proportions while making the grid the common reference. OBJ files are also assumed Y-up, which is what every common exporter writes, and rotated into the Z-up world on load.
- **Left-drag pans the viewport**, alongside middle-drag, so the obvious gesture moves the view. The cost is that the viewport captures every left press inside it, which is the button the authoring tools will eventually want for placing and selecting. When the tools start editing, one of the two has to give: a held modifier, an explicit pan mode, or middle-drag-only panning. That is a deliberate choice for now, not an oversight.
- **The viewport camera pans and zooms; it never rotates.** The projection is fixed at zero yaw and 4:3 dimetric foreshortening — axis-aligned 64×48 tiles with the height axis running straight up the screen — matching [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md). A rotating editor camera would show the world at angles no sprite is authored for.
- **A menu command that cannot work is shown, not hidden.** Save, Undo, Redo, Cut/Copy/Paste, and Settings are listed and greyed out; only commands the editor can carry out today are enabled (`editor-menu-command.h`). A greyed row is a promise about the roadmap; a missing row is a lie about it. The same rule gates Close Project on whether a project is open.
- **An accelerator hint is only shown when the key works.** The View commands display `0`, `=`, `-`, and `G` because `SimplishEditor::onClientKeyDown` handles them. Nothing else shows a hint, because nothing else has a binding — a hint for a dead key is worse than no hint.
- **`isoTimestampNow()` is editor-only.** It reads the wall clock, which simulation code may never do ([ADR-002](../decisions/ADR-002-fixed-timestep-determinism.md)). Project metadata timestamps are an editor concern and stay on the editor side of that line.

The editor exists because levels are hand-authored. Every design decision in [Game REQUIREMENTS](../game/REQUIREMENTS.md) — enemy funnelling, spawn placement, wave pacing — depends on somebody shaping a specific space, and that shaping needs a tool with an immediate feedback loop.

---

## 2. Goals

1. **Same renderer, same simulation.** The editor viewport is the game viewport. No preview approximation that can disagree with the runtime.
2. **Playtest without a round trip.** A designer enters play from the current edit state, in the current camera position, and returns to editing with the level unchanged.
3. **Text-diffable output.** Every artifact the editor writes is human-readable and merge-friendly. A level change must be reviewable in a pull request.
4. **Fast iteration over feature breadth.** A small set of tools that respond instantly beats a large set that require a rebuild.

---

## 3. Editor Shell

| Element | Requirement |
|---|---|
| Framework | The engine's retained-mode GUI ([Engine §6](../engine/REQUIREMENTS.md#6-core-systems)) — no third-party UI dependency |
| Layout | Dockable panels with persisted layout per workspace |
| Workspaces | Level, Encounter, Data, Assets — each a saved panel arrangement and tool set |
| Viewport | The isometric renderer with editor overlays: grid, spawn volumes, collision, flow-field visualisation, trigger regions |
| Undo/redo | Command-based, unbounded within a session, covering every mutating operation |
| Project model | A project is a directory of data files plus a manifest; the editor never owns a binary project database |
| Hot-reload | External changes to data files are detected and reloaded without restarting the editor |

---

## 4. Level Authoring

### 4.1 The Grid

Levels are built on the isometric tile grid: a 2D lattice of cells with terrain type, height, and traversal properties. Whether the grid supports multiple height levels is an open question ([Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions)) — the level format reserves a height field either way.

| Tool | Behaviour |
|---|---|
| Tile paint | Brush with adjustable size and shape; paints terrain type onto cells |
| Height paint | Raises and lowers cell height where verticality is supported |
| Fill and select | Rectangular, lasso, and flood selection; operations apply to selections |
| Auto-tiling | Edge and corner variants resolved automatically from neighbour rules, authored per tileset |
| Structure placement | Wall, doorway, and cover meshes snapped to grid edges |

### 4.2 Props and Entities

Free placement in continuous world space with optional grid snapping. Props carry a transform, a variant index, and collision derived from their definition. Entity placements — spawn points, objectives, interactables — carry a definition ID and a property block validated against that definition's schema.

### 4.3 Navigation and Flow

Because enemy movement runs on flow fields ([Game §5.2](../game/REQUIREMENTS.md#52-horde-ai)), the editor must show what those fields will do:

- Flow-field visualisation from any point, updating live as geometry changes
- Reachability highlighting — cells no enemy can reach are flagged, since they are almost always a mistake
- Chokepoint analysis showing where the horde will funnel
- Warnings for unreachable spawn volumes and objectives

### 4.4 Level Format

Levels serialise to JSON: a tile layer (run-length encoded for compactness while staying readable), a prop list, an entity list, spawn volumes, trigger regions, and metadata. Schema-validated on both write and load. A level file must diff sensibly — a change to one room produces a change to one region of the file.

---

## 5. Encounter and Wave Authoring

The director ([Game §6](../game/REQUIREMENTS.md#6-the-director)) executes what is authored here.

| Element | Requirement |
|---|---|
| Wave composition | Enemy archetypes with counts, weights, and spawn-volume assignments |
| Timing | Wave start conditions (time, trigger, previous-wave state), spawn cadence, duration |
| Intensity curve | A designer-drawn curve the director follows; pressure and relief are authored, not emergent |
| Spawn budget | Live display of the wave's projected cost against the engine's entity ceiling, with an over-budget warning before playtest |
| Player-count scaling | Per-wave scaling curves, previewable at 1, 2, 3, and 4 players |
| Adaptive band | The bounds within which the director may deviate from the authored curve |
| Boss scripting | Phase definitions with entry conditions, attack sets, and arena interaction |

Waves preview without entering play: scrubbing the timeline shows projected spawn locations and counts against the level geometry.

---

## 6. Data Editing

Structured editors over the JSON data tables — weapons, modifiers, enemy archetypes, projectile archetypes — driven by the same schemas the runtime validates against. Fields render according to their schema type, validation errors surface inline, and the underlying file stays canonical JSON that can be hand-edited or diffed.

Data edits hot-reload into a running playtest where the change is safe to apply mid-session; where it is not, the editor says so rather than applying it partially.

---

## 7. Playtest

The load-bearing feature.

| Requirement | Detail |
|---|---|
| Entry | Play from the current edit state — unsaved changes included — starting at the camera position or a chosen spawn point |
| Exit | Return to editing with the level exactly as it was; simulation state is discarded, edit state untouched |
| Simulation | The real game simulation, deterministic tick and all, with the real director |
| Debug overlays | Toggleable during play: collision, flow fields, spawn budget, entity counts, projectile counts, per-phase tick timing |
| Time control | Pause, single-step, and speed multipliers; stepping advances exactly one simulation tick |
| Replay | Every playtest records a replay ([Engine §4.4](../engine/REQUIREMENTS.md#44-replay)), reviewable in the editor with a free camera |
| Live tuning | CVar and data-table changes applied mid-playtest where safe |
| Multi-player preview | Simulate a 2–4 player session with AI-driven stand-ins to check density and scaling |

---

## 8. Asset Pipeline

| Stage | Requirement |
|---|---|
| Sprite atlasing | Offline packer producing atlas pages plus metadata (frame rects, pivots, per-direction sets). Runs as a build step and on demand from the editor |
| Sprite import | Directory conventions map to animation clips and the eight facing directions automatically |
| Mesh import | Static meshes for terrain and structures, with collision derived or authored |
| Audio import | Format conversion, loudness normalisation, and bank assembly |
| Validation | Every import validates against the runtime's expectations — atlas page limits, mesh vertex budgets, audio channel counts — and fails loudly at import rather than quietly at runtime |
| Determinism | The pipeline is reproducible: identical sources produce byte-identical outputs, so content hashes stay stable across machines |

> Whether sprites are hand-authored or pre-rendered from 3D models is open ([Project REQUIREMENTS §8](../../REQUIREMENTS.md#8-open-questions)). Pre-rendering adds a render step to this pipeline; the runtime side is unaffected.

---

## 9. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Editor frame rate | ≥ 60 FPS while editing a full-size level |
| Playtest entry | < 1 s from edit mode to simulating |
| Playtest exit | < 500 ms back to edit mode |
| Tile paint latency | Visible response within one frame of input |
| Level load | < 2 s for a full-size level |
| Hot-reload | Data-table change visible within 500 ms of file write |
| Autosave | Every 5 minutes and before every playtest, to a recoverable location |
| Crash recovery | An editor crash loses at most the autosave interval |
| Output format | All authored artifacts are text, schema-validated, and diff-friendly |

---

## 10. Milestone

**M9 — Editor.** Sequenced after the game is content-complete, so the editor is built against a settled runtime rather than a moving one. Two exceptions ship earlier because the game cannot be built without them:

- A minimal tile-and-prop placement tool in **M4**, sufficient to author the vertical-slice level
- Wave composition authoring in **M5**, sufficient to author the waves the director executes

Both are throwaway-grade and are replaced by the M9 tooling.

---

*This document is a living spec. The content this editor authors is specified in [Game REQUIREMENTS](../game/REQUIREMENTS.md); the rendering and simulation it embeds are specified in [Engine REQUIREMENTS](../engine/REQUIREMENTS.md).*
