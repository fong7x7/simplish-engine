# Simplish — Editor Requirements

**Parent document:** [REQUIREMENTS.md](../../REQUIREMENTS.md)
**Version:** 0.2
**Status:** First slice implemented — project loading, toolbar, viewport, and the level file props and lights are saved to
**Last Updated:** 2026-09-11

---

## 1. Overview

The editor is a desktop application that links `src/engine/` and `src/game/` and authors the content those layers consume: levels, encounters, waves, and the data tables behind weapons and enemies. It renders through the same isometric pipeline the game uses and simulates through the same deterministic tick, so what a designer sees in the editor is what ships.

**Desktop only** (macOS, Windows, Linux). Never built for consoles.

### Current State

A first slice builds and runs: `./build/debug/src/bin/editor/simplish-editor [project-directory]`.

| Package | Covers | State |
|---|---|---|
| `src/editor/project/` | The `.simplish/project.json` format, open and create, `last_opened_at` stamping, the recent-projects list | Built; 684 tests |
| `src/editor/shell/` | Title bar, menu bar (File / Edit / Level / View / Help), tool toolbar (Select / Tile / Height / Prop / Entity), the viewport with left- or middle-drag pan, scroll zoom, and click-to-select, the asset strip that drags models, light sources and player starts into the world, the properties panel that edits whichever is selected — a prop's behavior and faction among it — and the level file those placements are saved to and loaded from | Built; 679 tests |
| `src/editor/agent/` | The agent API: 37 tools over the shell's own state, the JSON protocol, and the binding to a running editor | Built; 121 tests |
| `src/platform/agent/` | The loopback HTTP transport that carries it | Built; 7 tests |
| `src/bin/editor/` | Entry point: resolves the data directory, opens a project given on the command line, opens the agent port when asked | Built |

What the slice deliberately does not do yet: almost nothing is authored. The viewport draws a grid but holds no tile data, and there is no dockspace. Those arrive with the level format (§4.4) and the sections below. What *is* authored — props, lights and player starts — now survives a restart; see the seventh exception below.

File > New Project and File > Open Project are live too, both through OS dialogs starting in the user's Documents folder — where a person keeps their own work — and falling back to home when there is no Documents to start in. New Project takes the folder name typed into a "save as" dialog as the project name and opens the result immediately, with its `.simplish/`, `data/`, `assets/`, and `content/levels/` directories in place. Open Project takes a folder and opens the project inside it, leaving the current one untouched when the folder turns out not to be one; the reason appears in the toolbar status line, because a log line is invisible to whoever just picked the wrong folder.

Asset placement is the second exception. The strip along the bottom lists the `.obj`, `.gltf` and `.glb` files under `<project>/assets/`, and dragging one onto the viewport loads it, uploads it, and draws it as a real depth-tested mesh on the tile it was dropped on. Everything dropped this way is written to the project's level file on save and comes back when the project is next opened — the seventh exception below.

A glTF file is a **rigged model**: a skinned mesh, its skeleton, and its animation clips ([animation.md](../engine/animation.md)). Dropped, it plays its first clip at once, looping on the viewport's own clock — in edit mode and during a playtest alike, since a pose is presentation the simulation never reads. Its properties end with an **Animation** row naming the clip it plays, whose step buttons move to the previous or next clip as one undoable edit, and the prop crossfades to it over a fifth of a second rather than snapping; the choice is saved with the prop by name. The model is sized, sat on the ground and picked by its bind pose's box, so a clip that swings an arm past that box does not change where the prop stands. A file that will not load says why in the status bar — the usual reasons are Draco compression, a skin over 80 joints, or no skin at all.

Selection and the properties panel are the third, and the first thing here that edits rather than places. Clicking a placed asset in the viewport selects it: its box is outlined in the accent colour, and a panel opens down the right listing the asset's name, six numbers — position X, Y and Z in tiles, rotation X, Y and Z in degrees — and a scale. Each number row has a step button either side and a value box that scrubs when dragged, so a prop is nudged a quarter tile or turned fifteen degrees by clicking, or moved continuously by dragging. Scale's row has the same buttons around a slider instead of a value box. Clicking bare ground, or pressing Escape, drops the selection and gives the viewport the panel's width back; Backspace or Delete removes what is selected, which Edit > Delete also does and which greys when nothing is selected. The three decisions inside it:

- **Picking is a ray test against boxes, not against meshes.** The editor does not keep mesh data on the CPU after upload, and a box around a prop is what the viewport already outlines — so what is picked is what is drawn. Where two boxes overlap on screen, the one nearest the camera along the projection's own ray wins, which is the ordering the depth buffer gives the meshes themselves: clicking a prop standing in front of another picks the one in front. The ray is the oblique projection's collapse direction, four tiles along world Y for every three up world Z, derived in `iso-view-matrix.h`.
- **A gesture is one history entry.** Dragging a value reports every intermediate number as a preview, which moves the prop live, and one final number as a commit, which is what `EditorActionKind::TRANSFORM_PLACEMENT` records against the placement as it was when the drag began. A drag across two hundred pixels is one Ctrl+Z, and a gesture that ended where it began records nothing at all. Undo and redo carry the selection with them — a placement that comes back is not the placement that slid into its slot, and a move that is reverted should be visible when it happens.
- **A prop's scale is one number, on a logarithmic slider.** Uniform rather than per axis, because every mesh shader carries the normal through the model matrix, which is only correct while each axis scales alike — stretching one would shade the model wrong on every backend until each grew a normal matrix. It multiplies the one-tile fit a dropped model gets, so 1 is the size it was dropped at, and it is held to an eighth through eight: zero would make a prop unpickable and a negative scale would turn it inside out. The slider is logarithmic because scale is felt as a ratio — 1 is its exact middle, and halving and doubling are the same distance either side — and pressing it jumps there as a slider does. Its step buttons move between fixed stops a quarter of a doubling apart rather than by an amount, which is how a value dragged to 1.03 steps back to exactly 1. Like rotation it applies about the footprint's centre at ground level, and the box the viewport outlines, picks against and the playtest collides with grows with it.
- **A rotation turns the prop where it stands.** The Euler angles are applied about the centre of the model's footprint at the height it rests on, so turning a crate spins it on its tile rather than swinging it across the grid. Zero rotation leaves the transform the plain scale-and-offset it was before, so nothing already placed moved when this arrived.

Built-in content is the fourth, and the first thing the browser offers that is not a file. Above the assets root, the folder pane lists a **general** section — built into the editor, the same in every project — divided into **lighting**, **shapes** and **tools**. It sits above rather than below because it is the same short list everywhere, while the assets root is a tree that grows, and it is divided because several kinds of built-in thing in one grid of cards read as a pile rather than as a choice. `tools` holds the player start — the ninth exception below.

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

Undo and redo are the sixth. Every placement, every light, every transform of either, and every removal of one is recorded as an action (`editor-action.h`), and Edit > Undo and Edit > Redo walk that history — the mechanism §3 asks for, built against the mutating operations that exist today rather than deferred until there are many. Actions are tagged value records, not command objects, and the history is one list with a cursor rather than two stacks; adding a tool means adding a kind and its inverse, and the compiler names every switch that has to grow — which is what removal was: two kinds whose inverse reinserts the entry the action carried away, so an undone delete gives back the prop that was there rather than a fresh one at its index. The history is per-session and dies with the document it describes. A rescan clears it too, but no longer takes the document with it: an action names an entry by an index into a list the rebind above may have shortened, while the entries themselves are rebound by id and survive. Ctrl+Z and Ctrl+Shift+Z run them from the keyboard, and Command works in place of Control on every platform rather than only on macOS — the engine's own clipboard keys already accept either, and someone arriving from the other convention should not have to find out which one this editor picked. They are also the only keys here that act on OS key-repeat: holding the accelerator to walk back through a run of edits is most of what the gesture is for, and undo stops on its own when the history runs out.

The agent API is the sixth, and the first thing here that is not for a person at all. Started with `SIMPLISH_AGENT_PORT` set, the editor listens on 127.0.0.1 and answers thirty-two tools — everything the interface can do to a level, plus everything it knows about one — over HTTP, and over MCP through a bridge that builds its tool list from the editor's own manifest. An agent asked to shift the only light source ten tiles right reads `list_lights` and calls `translate`, and what it does lands in the same document, in the same undo history, in the frame it arrived. The full contract is [agent-api.md](agent-api.md); what is and is not reachable through it is [capabilities.md](capabilities.md). Four decisions:

- **The tool surface is a pure function of shell state.** `runAgentTool` takes an `EditorShellState` and one call and gives back a result. It holds no pointer to the editor and opens no socket, which is why every tool in it is tested with no window and no GPU — and why the things that genuinely need the running editor (a camera move, opening a project, a rescan, switching level) are handed back as a value the editor carries out rather than reached for through a widget.
- **The shell does not know an agent exists.** `SimplishEditor` grew one generic extension point — a callback run once a tick with mutable shell state — and nothing naming agents, tools, or ports. That callback returns whether it changed anything, because rebuilding the properties panel every tick would tear a drag in progress out from under the pointer.
- **A tool is described once.** The table in `agent-tool-info.h` generates the HTTP manifest, and the MCP bridge reads that manifest at run time, so there is no second list of tools anywhere to fall behind. Adding a tool without a schema row, a dispatch entry, or a wire name fails the build; adding a menu command, a property field, or a toolbar tool without an agent name fails it too.
- **The port is opt-in.** A local port that edits somebody's project is not something to open for everyone who launches the editor. Unset, the editor runs exactly as it always did; set, it binds loopback and nothing else, which is the whole of its access control.

Saving is the seventh, and the first thing here that outlives the process. File > Save — and `Ctrl`/`Cmd`+S, which runs the same command — writes what has been placed and what lights it to `content/levels/<id>.level.json`, the first content file of the format §4.4 specifies; opening a project reads it back and puts everything in it back in the viewport, meshes uploaded, ready to select and edit. The row is greyed with no project open, because there would be nowhere to put the file. Three decisions:

- **A prop names its asset by reference, not by index.** `mesh:props_crate`, `shape:cube` — the same `kind:id` strings the rest of the format uses. The index a session holds an asset at is renumbered by any rescan, so a level saved with indices would decay the moment a file was added beside it. A prop whose asset the project no longer holds is dropped on load and counted in the log, the same rule a rescan already applies: one deleted `.obj` costs that prop and nothing else.
- **What is written is what the editor holds, not what the spec sketches.** A prop carries three rotation angles rather than the `yaw_steps` integer of [project-format.md §4](project-format.md#4-level-files), because the properties panel edits free degrees and rounding them away on save would throw somebody's authored value out. Lights get an array of their own, which that section predates. Both differences are recorded in [§4.1 of that document](project-format.md#41-what-the-editor-writes-today), with the migration the snap will need when it arrives.
- **Loading is not an edit.** The document is replaced and the undo history stays empty, so the first Ctrl+Z after opening a project does nothing rather than unwinding the file that was just read. Reading happens after the asset scan, because a prop can only bind by id to a list that has been built.
- **The name carries an asterisk while the file is behind.** `Demo *` in the title bar and in the toolbar, gone the moment a save lands — and gone again if every edit since the last save is undone, because the document is then the one in the file and an asterisk on it would be a lie. What the marker reads is not a flag but a position: the undo cursor as it stood when the level was last written, compared against where it stands now. That lives on the undo history rather than on the shell because `performEditorAction`, `undoEditorAction` and `redoEditorAction` are the three functions every change goes through, the panels' and the agent's alike, so nothing has to be remembered at a call site. Making a *different* edit after undoing past the save point discards the redo tail the save was recorded in, and the position is dropped with it: the cursor would otherwise land on the same number describing a different document.
- **A level file that will not parse is not saved over.** The editor cannot show what it could not read, so what a save would write is an empty level on top of whatever the file actually holds. Save refuses, with the reason in the status line, until the project is reopened successfully. A mistyped hand edit costs a session, not the level.

Levels are the eighth, and what turns a project from a document into a folder of them. The Level menu lists every `*.level.json` under `content/levels/`, in id order, with a mark on the one being edited; choosing another reads that file and replaces the document, the selection and the undo history together, because all three describe the level being closed. Level > New Level asks for a name, turns it into an id the format would accept — `Transit Station` becomes `transit_station`, since an id is what generated C++ will use ([project-format.md §3](project-format.md#3-conventions-common-to-every-file)) — and writes an empty level file before switching, so a level that could not be written is not one the editor is left sitting in. Three decisions:

- **A switch with unwritten edits is refused, not resolved.** The menu says so in the status line rather than saving on the way out or dropping the work; an agent has to pass `"unsaved": "discard"` in as many words. Opening a *project* still drops the document, which is the older behaviour and the more surprising one — it is the next thing to fix here, not a rule this follows.
- **A level exists once its file does.** The empty level a new project opens at is listed while it is open and gone once it is not, because nothing was written for it. That is also why New Level writes first and switches second.
- **`main` is a starting point, not a constant.** A project opens on `main` when it has one and on the first level it does have otherwise, so a project whose levels are named something else opens on one of them rather than on an empty level it does not hold.

Player starts are the ninth, and the first thing here that is authored for the game rather than drawn in the scene. General › tools holds a **Player Start** card; dragging it onto the viewport marks the tile it lands on as where a player spawns, feet on the middle of the tile. A session holds up to four players ([Game §8](../game/REQUIREMENTS.md#8-co-op)), so each start names one of them, and the properties panel lists that first — a Player row that steps 1 to 4 — then the start's position, then a **Character** row naming who its player plays as unless they pick someone else. Saved as the level format's first entity, `entity:player_start` with a `player` property ([project-format.md §4.1](project-format.md#41-what-the-editor-writes-today)), and reachable through `add_player_start` and `list_player_starts`. Three decisions:

- **A dropped start takes the lowest player without one.** Dropping four in a row gives players one to four without touching the panel; a fifth goes to player 1 again, because a second start for a player is a legitimate thing to author — the game can pick between them — and the lowest is the least surprising to hand out.
- **Four is the simulation's number, not the editor's.** The slot count is `sim::MAX_PLAYERS`, the size of the input frame the tick consumes ([simulation.md](../engine/simulation.md)), so the editor cannot author a start for a player the game has no input for.
- **A start is drawn whole, in its player's colour.** Where a prop shows its footprint under geometry, a start shows a person-height column — green, orange, violet, magenta for players one to four — so the four read as four players at a glance, and the selection outline replaces the colour while one is selected.
- **A start names a default character, not a model.** Characters are content — a data table, `content/data/characters.data.json`, each with a name, a model and the stats a player plays by ([project-format.md §8.1](project-format.md#81-what-the-editor-reads-today)) — and a start's Character row steps through **None** and every character in it, saved as the start's `character` property (`character:scout`). It is the default a level gives a player, not a lock: the selector opens on it, and the pick made there is what the run uses. The character's model stands in the column, as tall as a player is to collision (`game::PLAYER_HEIGHT_TILES`) whatever unit it was made in, facing +X as a spawning player aims. The table is edited by hand for now — the data-editing panel of §6 is where it moves — and read when the project opens, on a rescan, and on every Play.

Playtest is the tenth, and the first thing here that runs the game rather than authoring it — the load-bearing feature of §7, in a first slice. The toolbar ends with a **Play** button (Level › Play Level, or F5, does the same); pressing it builds the game's own `GameWorld` from the level and steps it in the real deterministic tick ([simulation.md](../engine/simulation.md)) at 60 Hz of real time, with player 1 standing on the level's first start for player 1 — or on the tile under the camera when there is none. WASD or the arrow keys move relative to the camera — W is up the screen, which in the isometric view is a diagonal across the grid — the cursor aims, the left button holds fire; the camera follows the player; the toolbar reads **Stop** and the status line shows the tick. Stop, Esc or F5 again throws the game away and leaves the level exactly as it was. Through the agent API, `start_playtest`, `send_input`, `get_playtest` and `stop_playtest` do the same, and `send_input` makes a playthrough exact: the same queued inputs from the same level end on the same tick hash.

When the project defines two or more characters, Play first opens the **character selector** over the viewport — the game's pre-run pick, in the editor: one card per character with its model's picture, name, speed and health, opening on the one player 1's start names, or the first. A click on a card plays as it; the arrows and Enter do the same from the keyboard; Esc, F5 or a click off the panel puts it away. With one character Play goes straight to it, and with none to the default character. `start_playtest` never shows it — it takes a `character`, or plays as the one the selector would open on. Five decisions:

- **The game runs from a copy, so there is nothing to restore.** A playtest is built from the document when Play is pressed and destroyed when it stops; while it runs, every editing gesture and every editing tool is refused. §7's "return to editing with the level exactly as it was" is then a property of the design rather than something undo has to achieve.
- **The editor drives the game, and the game knows nothing about the editor.** `src/game/world` takes a `GameSetup` — a seed, a player count, spawn positions, each player's character — and a `GameContent` holding the character table, and the editor turns its player starts, the selector's pick and its characters table into them. The same setup is what a client, a CI test, or ADR-008's entry states will hand it.
- **A character is a build, so the pick is simulation input.** Its speed and health are copied into the player at spawn, hashed with the rest of their state, and the pick is written into the replay header beside the seed — so a run replays with the stats it was played with, and two co-op peers who agree on the setup agree on every tick. A pick naming a character the content lacks plays as the default character, the same way on every machine. Loadouts join the character when weapons exist to put in one.
- **Input stops being a float at the edge.** `engine/input` turns held keys and the cursor's direction into a quantised `PlayerInput`, and that integer record is what the tick runs on, the replay stores, and `send_input` writes. A keyboard and an agent pressing the same thing send the same bits.
- **The player is drawn as their character.** Their character's model, turned to face where they aim — a model is taken to face its own +Z, glTF's convention — and, when it is rigged, playing the first clip whose name has `run` or `walk` in it while moving and `idle` in it while standing, crossfading between them as a prop's clips do. The default character, or one with no model, gives the built-in cylinder at a person's proportions. Either is lit and shaded like everything else, with a column in the player's colour over it. Props stop the player: each is a solid box in the playtest — the box the viewport outlines — and the player, an upright cylinder a little narrower than a tile, stops against it and slides along it (`engine/physics`). A prop's properties end in a **Collides** checkbox, ticked for every prop dropped, which a click anywhere on its row flips; one that is unticked is drawn with a faded footprint and walked through, for the grass, the rugs and the decals.

Every playtest records a replay and writes it to `data/playtests/<level>.replay` when it stops. Players do not collide with each other, the ground has no height to follow, and there is no spatial index yet — every prop is tested every tick, which is fine for a level's props and a handful of players. A playtest pauses and resumes with F6 (Level › Pause Playtest) and steps exactly one tick with F7, on the input held or queued — `step_playtest` runs any number of ticks and leaves it paused, which is how an agent lands on an exact tick. Speed multipliers, replay review, scaling curves for the multi-player preview and §7's other debug overlays are not built; [capabilities.md §6.1](capabilities.md#61-playtest) is the register.

Actors are the eleventh, and the first thing here that the game decides for itself. Any placed prop can be given a **behavior** — its properties end with a **Behavior** row stepping through None, the game's built-in behaviors (Idle, Wander, Guard, Chase, Skirmisher, Coward, Follower, Charger, Patrol, Defender, Spitter, Bloater) and the project's own from `content/data/behaviors.data.json` ([project-format.md §8.2](project-format.md#82-the-behaviors-table)) — and, once it has one, a **Faction** row: hostile, neutral or friendly. A prop with a behavior is an **actor**: pressing Play turns it into an actor in the game's simulation ([actors.md](../game/actors.md)), which sees and hears the players, plans a path round the level's props, turns and moves as its behavior's states say, and is drawn wherever the game has it, turned to face its way, playing its state's clip or its walk and idle clips. In the viewport its footprint is outlined in its faction's colour — red, stone, teal — with a tick the way it faces, while editing and while playing. Saved with the prop as `behavior` and `faction` (and `route`, below), and reachable through `set_behavior`, `list_behaviors`, `list_placements` and `get_playtest`, which reports every actor's position, facing, state and target. Four decisions:

- **Intelligence is chosen per prop, not per entity.** The prop already has the model, the transform, the scale and the clip an actor needs; a separate entity kind would duplicate all four and the panel rows that edit them. A prop with a behavior is not a collision box for players in a playtest — its body is the actor's — whatever its Collides box says.
- **A behavior is data, from closed sets** ([ADR-009](../decisions/ADR-009-actor-behavior-state-machines.md)). States each do one action; exits each test one condition; nothing is scripted. The table is edited by hand until the data-editing panel of §6 exists, and read when the project opens, on a rescan, and on every Play; a reference to a behavior the project no longer has is kept, and plays as idle.
- **The panel's choice rows are named, not counted.** A rigged prop with a behavior shows Animation, Behavior, Faction and Route; a start shows Character. Each row carries an `EditorChoiceKind`, so an edit reaches the row it came from whichever of them are showing.
- **Facing starts from the prop's rotation.** A model is taken to face its own glTF +Z — world −Y after the Z-up turn, as a player's model is — so the actor starts facing its Z rotation minus a quarter turn, handed to the game in degrees and turned into a direction by the simulation's own deterministic trigonometry.

Seeing what actors think is the twelfth. The View menu's **Navigation Overlay** shades, over the level, every quarter-tile cell an actor cannot use — solid, too narrow for an actor a player's width, or open but walled off from every player start — from the same navigation grid the game builds, and the status line names any actor no path joins to a player start; `get_navigation` reports the same, and `find_path` plans the route an actor of any width would take between two points, by the game's own A* and smoothing. The **AI Overlay** draws, while a playtest runs, each actor's view cone, the rest of the path it is walking, a line to the target it sees, and its state's name over its head. A route for an actor to patrol is laid out with **waypoints**, the second card in general › tools: each belongs to one of nine routes and has a place in it, shown in its properties as Route and Order; dropping one while a waypoint is selected adds it to that route, after its last, so a route is laid out by dropping one after another, and the viewport joins each route's waypoints in walking order in the route's colour. An actor's **Route** row names the route its behavior's `patrol` state walks. Waypoints are saved as `entity:waypoint` entities and the route as the prop's `route`; `add_waypoint` and `list_waypoints` reach them, and the generic tools edit, move, select and delete them as they do everything else. Two decisions:

- **The editor measures the level with the game's grid.** `buildWorldNavGrid` is the one function that turns a setup's props into a navigation grid, and the editor calls it on the setup Play would build, so the overlay cannot disagree with where actors will actually walk.
- **A route belongs to the level, not to the actor.** Waypoints are their own entities numbered into routes, and an actor names a route; two guards can walk one route, and moving a waypoint moves every patrol that uses it.

Consequences are the thirteenth. A playtest's actors attack — bite, rush, fire volleys, lob hazard pools, burst ([actors.md §4](../game/actors.md#4-attacks-damage-and-death)) — and players lose health segments, go down and are revived by a teammate standing by them, or are put out; the status line gives player 1's health and says when they are down or the run is over, projectiles fly as small orange boxes and pools lie as green squares, and the AI Overlay labels every actor with its health. Level › Play with 1–3 Stand-ins — or `start_playtest`'s `stand_ins` — adds stand-in players to the next playtest, the multi-player preview of §7, each spawning on its player's start or beside player 1 and played by the game's own stand-in. Two decisions:

- **A stand-in is input, not a cheat.** It reads the world between ticks and gives the same quantised `PlayerInput` a keyboard gives, and the replay records it; the simulation never knows who was holding the stick. The same function will play a dropped co-op peer's character.
- **The setting is the editor's, not the level's.** How many stand-ins to play with is kept in `EditorShellState` and survives from one playtest to the next, but it is not saved with the level: it says how the designer wants to test, not what the level is.

The menu bar is the exception that proves the point. It is built — File, Edit, Level, View, and Help, with dropdowns, separators, accelerator hints, and recent projects — but most of what a menu bar traditionally offers has nothing behind it yet. Rather than hide those commands, the bar lists them disabled, so the menu reads as the shape of the editor rather than only the parts that happen to exist.

The decisions the slice locks in:

- **The grid is under the geometry, the cursor over it.** The viewport marks where the 3D scene composites into the GUI's paint order: grid, axes and placement footprints paint beneath it, so a model standing on a tile hides the lines it covers; the hover highlight paints above it, so cursor feedback stays visible over geometry.
- **A dropped model is scaled to one tile and stood on the ground.** Models arrive in whatever unit their author used, and a metre-scale crate beside a centimetre-scale one is unreadable. Scaling by the larger horizontal extent keeps each model's own proportions while making the grid the common reference. OBJ files are also assumed Y-up, which is what every common exporter writes, and rotated into the Z-up world on load.
- **A model is drawn with the diffuse map its material names.** The loader reads `mtllib` and `usemtl`, finds `map_Kd` in the library beside the model, and the editor decodes and uploads that image with the mesh; the viewport then shades the model with it. One map per model, not one per material — the renderer draws a mesh in a single call, and splitting a model into a draw per material is submesh work that is not done yet, so a model naming several materials takes the first. `map_Kd` is also the only map read: the shader has no material model to hang a specular or a normal map on, and reading them would be collecting data nothing can draw. A model with no material, or one whose image will not load, is drawn in the flat colour every model had before — which is one texel of a built-in stand-in texture, so the shader has no untextured branch in it.
- **Left-drag pans the viewport and left-click selects**, with middle-drag panning too. This is the resolution of the conflict the first slice left open: rather than a held modifier or an explicit pan mode, what separates the two is whether the pointer moved, decided on release. A press that travels more than a few pixels was a pan and changes nothing; one that does not was a click and picks. A pan that moves the view by three pixels is imperceptible, so nothing is lost to the slop, and neither gesture had to be given a modifier nobody would discover.
- **Selecting is not gated on the Select tool.** Every other tool in the toolbar is inert, so gating would make clicking do nothing in four modes out of five. When the other tools start editing, this is the line that moves.
- **The viewport camera pans and zooms; it never rotates freely.** A project chooses one of two fixed projections, and the View menu switches between them: **dimetric** (the default — zero yaw, 4:3 foreshortening, axis-aligned 64×48 tiles) or **isometric** (45° yaw, 2:1, 64×32 diamonds). Height runs straight up the screen in both, foreshortened by the camera's pitch like anything else a camera sees — the projections are rotations of the world, not shears of it, which is what makes a sphere draw round. The choice is stored in the project, not in an editor preference, because tile art is authored against it; see [ADR-003](../decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-08-projection-as-a-project-setting). A freely rotating camera would show the world at angles no sprite is authored for.
- **Shading is a project setting too, and switches between frames.** The View menu's **Smooth Shading** and **Cel Shading** rows choose how meshes are lit: smooth is the continuous light meshes have always had; cel flattens each light into three tones and draws a dark line along every silhouette and crease. It is the project's art style rather than a viewing preference, so it is written into `project.json` like the projection — but unlike the projection nothing authored depends on it, so it can be flipped freely and the next frame shows it. Asset thumbnails stay smooth: a card is for recognising an asset. The look itself is the engine's `MeshStyle` ([Engine §5.1](../engine/REQUIREMENTS.md#51-camera-and-projection)); the project names a preset rather than storing its numbers.
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

Props and lights are written and read today, to and from `content/levels/<id>.level.json`, where the id is the level the Level menu has open; tiles, entities and regions wait on the tools that author them ([project-format.md §4.1](project-format.md#41-what-the-editor-writes-today)).

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
| Mesh import | Static meshes for terrain and structures, with collision derived or authored. Rigged, animated characters come in as glTF 2.0 — built: `.gltf` and `.glb` with a skinned mesh are read by `engine/gltf`, clips and all ([animation.md §5](../engine/animation.md#5-loading-gltf)); static glTF and embedded images are not read yet |
| Mesh generation | Built, outside the editor: `tools/image-to-mesh.py` turns one image into a textured OBJ + MTL + PNG with a local Hunyuan3D 2.1 (`--setup` installs it, ~30 GB, Apple Silicon only). Its output is a *source* asset, kept beside its input image and a `.generation.json` of the settings — generation is not reproducible across machines, so it sits before this pipeline rather than in it, and the Determinism row applies from the OBJ on |
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
