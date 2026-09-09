# Simplish — Editor Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.2
**Status:** First slice implemented — project loading, toolbar, viewport, and the level file props and lights are saved to
**Last Updated:** 2026-09-09

---

## 1. Overview

The editor is a desktop application that links `src/engine/` and `src/game/` and authors the content those layers consume: levels, encounters, waves, and the data tables behind weapons and enemies. It renders through the same isometric pipeline the game uses and simulates through the same deterministic tick, so what a designer sees in the editor is what ships.

**Desktop only** (macOS, Windows, Linux). Never built for consoles.

### Current State

A first slice builds and runs: `./build/debug/src/bin/editor/simplish-editor [project-directory]`.

| Package | Covers | State |
|---|---|---|
| `src/editor/project/` | The `.simplish/project.json` format, open and create, `last_opened_at` stamping, the recent-projects list | Built; 28 tests |
| `src/editor/shell/` | Title bar, menu bar (File / Edit / View / Help), tool toolbar (Select / Tile / Height / Prop / Entity), the viewport with left- or middle-drag pan, scroll zoom, and click-to-select, the asset strip that drags models and light sources into the world, the properties panel that edits whichever is selected, and the level file those placements are saved to and loaded from | Built; 456 tests |
| `src/editor/agent/` | The agent API: 22 tools over the shell's own state, the JSON protocol, and the binding to a running editor | Built; 62 tests |
| `src/platform/agent/` | The loopback HTTP transport that carries it | Built; 7 tests |
| `src/bin/editor/` | Entry point: resolves the data directory, opens a project given on the command line, opens the agent port when asked | Built |

What the slice deliberately does not do yet: almost nothing is authored. The viewport draws a grid but holds no tile data, and there is no dockspace. Those arrive with the level format (§4.4) and the sections below. What *is* authored — props and lights — now survives a restart; see the seventh exception below.

File > New Project and File > Open Project are live too, both through OS dialogs starting in the user's Documents folder — where a person keeps their own work — and falling back to home when there is no Documents to start in. New Project takes the folder name typed into a "save as" dialog as the project name and opens the result immediately, with its `.simplish/`, `data/`, `assets/`, and `content/levels/` directories in place. Open Project takes a folder and opens the project inside it, leaving the current one untouched when the folder turns out not to be one; the reason appears in the toolbar status line, because a log line is invisible to whoever just picked the wrong folder.

Asset placement is the second exception. The strip along the bottom lists the `.obj` files under `<project>/assets/`, and dragging one onto the viewport loads it, uploads it, and draws it as a real depth-tested mesh on the tile it was dropped on. Everything dropped this way is written to the project's level file on save and comes back when the project is next opened — the seventh exception below.

Selection and the properties panel are the third, and the first thing here that edits rather than places. Clicking a placed asset in the viewport selects it: its box is outlined in the accent colour, and a panel opens down the right listing the asset's name and six numbers — position X, Y and Z in tiles, rotation X, Y and Z in degrees. Each row has a step button either side and a value box that scrubs when dragged, so a prop is nudged a quarter tile or turned fifteen degrees by clicking, or moved continuously by dragging. Clicking bare ground, or pressing Escape, drops the selection and gives the viewport the panel's width back. The three decisions inside it:

- **Picking is a ray test against boxes, not against meshes.** The editor does not keep mesh data on the CPU after upload, and a box around a prop is what the viewport already outlines — so what is picked is what is drawn. Where two boxes overlap on screen, the one nearest the camera along the projection's own ray wins, which is the ordering the depth buffer gives the meshes themselves: clicking a prop standing in front of another picks the one in front. The ray is the oblique projection's collapse direction, four tiles along world Y for every three up world Z, derived in `iso-view-matrix.h`.
- **A gesture is one history entry.** Dragging a value reports every intermediate number as a preview, which moves the prop live, and one final number as a commit, which is what `EditorActionKind::TRANSFORM_PLACEMENT` records against the placement as it was when the drag began. A drag across two hundred pixels is one Ctrl+Z, and a gesture that ended where it began records nothing at all. Undo and redo carry the selection with them — a placement that comes back is not the placement that slid into its slot, and a move that is reverted should be visible when it happens.
- **A rotation turns the prop where it stands.** The Euler angles are applied about the centre of the model's footprint at the height it rests on, so turning a crate spins it on its tile rather than swinging it across the grid. Zero rotation leaves the transform the plain scale-and-offset it was before, so nothing already placed moved when this arrived.

Built-in content is the fourth, and the first thing the browser offers that is not a file. Above the assets root, the folder pane lists a **general** section — built into the editor, the same in every project — divided into **lighting** and **shapes**. It sits above rather than below because it is the same short list everywhere, while the assets root is a tree that grows, and it is divided because two kinds of built-in thing in one grid of cards read as a pile rather than as a choice.

`shapes` holds a cube, a cylinder, a pyramid and a sphere, generated rather than loaded (`mesh-primitives.h`) and each built in the same unit box, so a dropped shape is exactly one tile across and stands on the ground like any model. They are the geometry to block a level out with before there are props to fill it, and to test lighting against.

`lighting` holds a directional light and a point light. Dragging one onto the viewport is the same gesture as dragging a model, and it adds a light the scene pass then shades every mesh by: a directional light for the sun, aimed by a direction vector, and a point light that hangs over the tile it was dropped on and falls off to nothing at its range. Both carry an intensity and an RGB tint, and the properties panel lists whichever numbers the kind actually uses — a directional light has no range to set, and a point light has no direction. Three decisions here:

- **A scene with no lights is lit exactly as it was.** The renderers substitute one built-in key light — the direction, strength and colour every mesh has been drawn with since meshes first drew — so lighting a level is something a designer opts into rather than something they must do before the viewport is readable. A light left at its defaults reproduces that key light, so the first one dropped changes the picture as little as possible.
- **A shape is an asset; only a light is not.** The shapes go on the end of the editor's asset list, so a placement names one by the same index it names a scanned model by and the transform, the marker, the properties panel and the scene pass need to know nothing about where the geometry came from. What marks a shape is a field on the asset saying which one it is, which is what sends the mesh to the generator instead of the OBJ loader — and to a thumbnail rendered on the spot instead of one cached against a file.
- **The browser numbers built-in items after the assets.** A folder holds entry numbers, and the lights' are the ones past every asset, so a drop reports one number and the editor tells a light from a model by where it falls. The browser itself knows nothing about either.
- **Both renderers shade the same way.** The Metal fragment shader and the CPU rasterizer (`mesh-rasterizer.h`, which draws card thumbnails and every capture test) compute the same ambient-plus-lambert term over the same light records, so what a test asserts is what the viewport shows. The record's layout is asserted at compile time, because a field reordered on the C++ side is read silently wrong on the GPU one.

This is not the lighting model of [Engine §5.4](../engine/REQUIREMENTS.md#54-lighting) — there is no shadowing, no clustering, and the shader's list is eight lights long rather than 256. It is what the editor needs to author light placement against, and what the level format will carry when it arrives.

Stable ids are the fifth, and the first piece of the level format ([§4.4](#44-level-format)) to land ahead of the format itself. Everything the editor can name now carries one: an asset takes its id from its own path with the extension dropped (`props/crate.obj` becomes `props_crate`), a built-in shape takes its own name (`cube`), and everything placed is numbered from what it instances (`props_crate_01`, `point_01`). The properties panel shows the selected thing's reference under its name — `prop:props_crate_01` — which is the string a level or logic file will write to mean it, so it can be copied rather than assembled. The agent API reports the same `id` and `ref` on every asset, placement and light, and accepts either where it used to take only an index or a display name.

Three things follow from that, and they are why ids arrived before the format that needs them:

- **An asset id depends on the asset's own path and nothing else.** A numeric suffix handed out in scan order would look stable and not be: adding `debris/crate.obj` to a project would renumber `props/crate.obj`, and every level naming the old id would then be wrong. Path derivation costs a longer id and buys an id that only changes when the file moves. The one case that still depends on scan order is two paths that slugify alike — `my-crate` and `my_crate` — where the later takes a suffix.
- **A placement id is not a position in a list.** It is minted when the thing is placed, from the lowest free number for that asset, and it travels in the undo record — so undoing a placement frees its id and redoing restores the same one. This is what logic will reference: a trigger opens *that* gate, not whatever currently sits at index four.
- **A rescan no longer costs the level.** Placements name their asset by id, so the rescan rebinds them to the new numbering instead of dropping them, and adding a file to a project keeps everything already placed. A placement whose asset is genuinely gone is dropped — there is no mesh to draw and no id to resolve — and the undo history goes with the rescan either way, since it describes the document by index into a list that may have just got shorter.

Undo and redo are the sixth. Every placement, every light, and every transform of either is recorded as an action (`editor-action.h`), and Edit > Undo and Edit > Redo walk that history — the mechanism §3 asks for, built against the mutating operations that exist today rather than deferred until there are many. Actions are tagged value records, not command objects, and the history is one list with a cursor rather than two stacks; adding a tool means adding a kind and its inverse, and the compiler names every switch that has to grow. The history is per-session and dies with the document it describes. A rescan clears it too, but no longer takes the document with it: an action names an entry by an index into a list the rebind above may have shortened, while the entries themselves are rebound by id and survive. Ctrl+Z and Ctrl+Shift+Z run them from the keyboard, and Command works in place of Control on every platform rather than only on macOS — the engine's own clipboard keys already accept either, and someone arriving from the other convention should not have to find out which one this editor picked. They are also the only keys here that act on OS key-repeat: holding the accelerator to walk back through a run of edits is most of what the gesture is for, and undo stops on its own when the history runs out.

The agent API is the sixth, and the first thing here that is not for a person at all. Started with `SIMPLISH_AGENT_PORT` set, the editor listens on 127.0.0.1 and answers twenty-two tools — everything the interface can do to a level, plus everything it knows about one — over HTTP, and over MCP through a bridge that builds its tool list from the editor's own manifest. An agent asked to shift the only light source ten tiles right reads `list_lights` and calls `translate`, and what it does lands in the same document, in the same undo history, in the frame it arrived. The full contract is [agent-api.md](agent-api.md); what is and is not reachable through it is [capabilities.md](capabilities.md). Four decisions:

- **The tool surface is a pure function of shell state.** `runAgentTool` takes an `EditorShellState` and one call and gives back a result. It holds no pointer to the editor and opens no socket, which is why every tool in it is tested with no window and no GPU — and why the three things that genuinely need the running editor (a camera move, opening a project, a rescan) are handed back as a value the editor carries out rather than reached for through a widget.
- **The shell does not know an agent exists.** `SimplishEditor` grew one generic extension point — a callback run once a tick with mutable shell state — and nothing naming agents, tools, or ports. That callback returns whether it changed anything, because rebuilding the properties panel every tick would tear a drag in progress out from under the pointer.
- **A tool is described once.** The table in `agent-tool-info.h` generates the HTTP manifest, and the MCP bridge reads that manifest at run time, so there is no second list of tools anywhere to fall behind. Adding a tool without a schema row, a dispatch entry, or a wire name fails the build; adding a menu command, a property field, or a toolbar tool without an agent name fails it too.
- **The port is opt-in.** A local port that edits somebody's project is not something to open for everyone who launches the editor. Unset, the editor runs exactly as it always did; set, it binds loopback and nothing else, which is the whole of its access control.

Saving is the seventh, and the first thing here that outlives the process. File > Save — and `Ctrl`/`Cmd`+S, which runs the same command — writes what has been placed and what lights it to `content/levels/main.level.json`, the first content file of the format §4.4 specifies; opening a project reads it back and puts everything in it back in the viewport, meshes uploaded, ready to select and edit. The row is greyed with no project open, because there would be nowhere to put the file. Three decisions:

- **A prop names its asset by reference, not by index.** `mesh:props_crate`, `shape:cube` — the same `kind:id` strings the rest of the format uses. The index a session holds an asset at is renumbered by any rescan, so a level saved with indices would decay the moment a file was added beside it. A prop whose asset the project no longer holds is dropped on load and counted in the log, the same rule a rescan already applies: one deleted `.obj` costs that prop and nothing else.
- **What is written is what the editor holds, not what the spec sketches.** A prop carries three rotation angles rather than the `yaw_steps` integer of [project-format.md §4](project-format.md#4-level-files), because the properties panel edits free degrees and rounding them away on save would throw somebody's authored value out. Lights get an array of their own, which that section predates. Both differences are recorded in [§4.1 of that document](project-format.md#41-what-the-editor-writes-today), with the migration the snap will need when it arrives.
- **Loading is not an edit.** The document is replaced and the undo history stays empty, so the first Ctrl+Z after opening a project does nothing rather than unwinding the file that was just read. Reading happens after the asset scan, because a prop can only bind by id to a list that has been built.
- **The name carries an asterisk while the file is behind.** `Demo *` in the title bar and in the toolbar, gone the moment a save lands — and gone again if every edit since the last save is undone, because the document is then the one in the file and an asterisk on it would be a lie. What the marker reads is not a flag but a position: the undo cursor as it stood when the level was last written, compared against where it stands now. That lives on the undo history rather than on the shell because `performEditorAction`, `undoEditorAction` and `redoEditorAction` are the three functions every change goes through, the panels' and the agent's alike, so nothing has to be remembered at a call site. Making a *different* edit after undoing past the save point discards the redo tail the save was recorded in, and the position is dropped with it: the cursor would otherwise land on the same number describing a different document.
- **A level file that will not parse is not saved over.** The editor cannot show what it could not read, so what a save would write is an empty level on top of whatever the file actually holds. Save refuses, with the reason in the status line, until the project is reopened successfully. A mistyped hand edit costs a session, not the level.

The menu bar is the exception that proves the point. It is built — File, Edit, View, and Help, with dropdowns, separators, accelerator hints, and recent projects — but most of what a menu bar traditionally offers has nothing behind it yet. Rather than hide those commands, the bar lists them disabled, so the menu reads as the shape of the editor rather than only the parts that happen to exist.

The decisions the slice locks in:

- **The grid is under the geometry, the cursor over it.** The viewport marks where the 3D scene composites into the GUI's paint order: grid, axes and placement footprints paint beneath it, so a model standing on a tile hides the lines it covers; the hover highlight paints above it, so cursor feedback stays visible over geometry.
- **A dropped model is scaled to one tile and stood on the ground.** Models arrive in whatever unit their author used, and a metre-scale crate beside a centimetre-scale one is unreadable. Scaling by the larger horizontal extent keeps each model's own proportions while making the grid the common reference. OBJ files are also assumed Y-up, which is what every common exporter writes, and rotated into the Z-up world on load.
- **Left-drag pans the viewport and left-click selects**, with middle-drag panning too. This is the resolution of the conflict the first slice left open: rather than a held modifier or an explicit pan mode, what separates the two is whether the pointer moved, decided on release. A press that travels more than a few pixels was a pan and changes nothing; one that does not was a click and picks. A pan that moves the view by three pixels is imperceptible, so nothing is lost to the slop, and neither gesture had to be given a modifier nobody would discover.
- **Selecting is not gated on the Select tool.** Every other tool in the toolbar is inert, so gating would make clicking do nothing in four modes out of five. When the other tools start editing, this is the line that moves.
- **The viewport camera pans and zooms; it never rotates freely.** A project chooses one of two fixed projections, and the View menu switches between them: **dimetric** (the default — zero yaw, 4:3 foreshortening, axis-aligned 64×48 tiles) or **isometric** (45° yaw, 2:1, 64×32 diamonds). Height runs straight up the screen in both, foreshortened by the camera's pitch like anything else a camera sees — the projections are rotations of the world, not shears of it, which is what makes a sphere draw round. The choice is stored in the project, not in an editor preference, because tile art is authored against it; see [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-08-projection-as-a-project-setting). A freely rotating camera would show the world at angles no sprite is authored for.
- **A menu command that cannot work is shown, not hidden.** Save, Cut/Copy/Paste, and Settings are listed and greyed out; only commands the editor can carry out today are enabled (`editor-menu-command.h`). A greyed row is a promise about the roadmap; a missing row is a lie about it. The same rule gates Close Project on whether a project is open, and Undo and Redo on whether the history has anything for them to do.
- **An accelerator hint is only shown when the key works.** The View commands display `0`, `=`, `-`, and `G`, and Edit shows the undo pair, because `SimplishEditor::onClientKeyDown` handles them. Nothing else shows a hint, because nothing else has a binding — a hint for a dead key is worse than no hint. The undo hint is the one that reads differently per platform: both modifiers work everywhere, so the text names the one that platform's users expect.
- **An agent gets the editor's vocabulary, not a smaller one.** The API names placements, lights, and menu commands — the things the editor holds — rather than offering clicks at pixel coordinates. What survives the interface being redrawn is what a tool is allowed to name.
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

Props and lights are written and read today, to and from `content/levels/main.level.json`; tiles, entities and regions wait on the tools that author them ([project-format.md §4.1](project-format.md#41-what-the-editor-writes-today)).

The full on-disk specification — levels, encounters, scenarios, logic, and data tables, and what each becomes when the game is built — is [project-format.md](project-format.md). Content is authored as JSON and compiled to generated C++ for shipping builds, while the editor and development builds load the JSON directly so playtest never waits for a compile ([ADR-007](../decisions/ADR-007-json-authored-cpp-baked-content.md)).

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

Structured editors over the JSON data tables — weapons, modifiers, enemy archetypes, projectile archetypes — driven by the same schemas the runtime validates against ([format](project-format.md#8-data-tables)). Fields render according to their schema type, validation errors surface inline, and the underlying file stays canonical JSON that can be hand-edited or diffed.

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
