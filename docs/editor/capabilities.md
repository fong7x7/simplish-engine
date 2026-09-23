# Simplish — Editor Capabilities

**Parent document:** [Editor REQUIREMENTS](REQUIREMENTS.md)
**Version:** 1.0
**Status:** Living register — update it in the change that moves a row
**Last Updated:** 2026-09-22 (painted ground)

What the editor can do, and whether an agent can do it too. One row per
capability, three states per row.

[REQUIREMENTS.md](REQUIREMENTS.md) says what the editor *must* eventually
do; this says what it does today and how far each thing reaches. The two
have different jobs: that document is argued over, this one is checked.

---

## 1. How to read a row

| Column | Means |
|---|---|
| **In the editor** | ✅ built · 🚧 partly · ❌ not yet |
| **Agent** | The tools in [agent-api.md](agent-api.md) that reach it, or ❌ when nothing does |
| **Notes** | What the gap is, when there is one |

**The rule.** A capability is not finished when the interface can do it. It
is finished when the interface can do it *and* the agent API can. A ✅ in the
first column with a ❌ in the second is a bug in the change that put it
there — see [agent-api.md §6](agent-api.md#6-adding-a-tool--the-rule).

The one exception is a gesture that is only a gesture: panning with a drag
has nothing an agent would say to it beyond the camera command that already
exists. Where that is the case the row says so rather than leaving a ❌ to be
puzzled over.

---

## 2. Project

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Open a project from a path | ✅ | `open_project` | |
| Open a project from a dialog | ✅ | `run_command` (`open_project`) | The dialog is the OS's; an agent wanting a path uses the tool above |
| Create a project | ✅ | `run_command` (`new_project`) | Dialog only — the name comes from what the user types, so there is no path-taking tool |
| Close a project | ✅ | `run_command` (`close_project`) | |
| Recent projects list | ✅ | ❌ | Reachable from the menu, not from the API. Add `list_recent` when something needs it |
| Read the open project | ✅ | `get_state` | Name, root, and whether one is open |
| Save a level | 🚧 | `run_command` (`save`) | Props, lights and player starts, to `content/levels/<id>.level.json`; `Ctrl`/`Cmd`+S runs the same command. Tiles, other entities and regions wait on the tools that author them — [project-format.md §4.1](project-format.md#41-what-the-editor-writes-today) |
| Load a level when a project opens | 🚧 | `open_project`, `list_placements`, `list_lights`, `list_player_starts` | Read after the assets are scanned, so a prop binds by asset id; a prop whose asset is gone is dropped and counted, and so is an entity whose definition the editor does not know |
| See whether the level has unsaved changes | ✅ | `get_level`, `get_state` | The project's name carries a trailing asterisk in the title bar and toolbar while it does |
| See where the level is written, and whether one is there | ✅ | `get_level` | Also whether the file could be read; a file that would not parse is not saved over |
| Save under another name | ❌ | ❌ | Nothing writes a level to a chosen path; a new level is created and edited instead. Listed in the menu, disabled |

## 2.1 Levels

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| List the project's levels | ✅ | `list_levels` | The Level menu lists them in id order and marks the one being edited. A level exists once its file does, so a level nothing has saved into is listed only while it is the open one |
| Create a level | ✅ | `create_level` | Level > New Level asks for a name and turns it into an id (`Transit Station` → `transit_station`); the file is written before the switch, so a level that could not be written is not one the editor is left in |
| Switch level | ✅ | `open_level` | Replaces the document, the selection and the undo history together — they all describe the level being closed |
| Keep unwritten edits from being lost | ✅ | `create_level`, `open_level` (`unsaved`) | The menu refuses the switch and says so; an agent has to pass `"unsaved": "discard"` in as many words |
| See which level is open | ✅ | `get_level`, `list_levels` | Also on the title bar and the toolbar, after the project's name |
| Rename or delete a level | ❌ | ❌ | An id is a reference every other content file will use ([project-format.md §3](project-format.md#3-conventions-common-to-every-file)), so renaming is a refactor across the project rather than a file move. Deleting is a filesystem operation with no undo behind it yet |

## 3. Assets

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Scan `.obj`, `.gltf` and `.glb` files under `assets/` | ✅ | `list_assets`, `rescan_assets` | OBJ is static; glTF is read as a rigged, animated model ([animation.md](../engine/animation.md)) |
| Import a rigged, animated model | ✅ | `list_assets`, `get_asset` (`rigged`, `clips`) | A `.gltf` or `.glb` with a skinned mesh loads when first placed, turned Z-up, with every clip it has. One that will not load says why in the status bar — Draco compression, over 80 joints, no skin — and `get_asset` reports `load_failed`. Static glTF and images embedded in a buffer are not read yet. Draws on every backend with a skinned pipeline — Metal, DX12, OpenGL |
| Browse them by folder | ✅ | `list_folders` | Includes the built-in section beside the project's own folders |
| Read one asset's state | ✅ | `get_asset` | Mesh loaded, load failed, thumbnail state, measured bounds, how many placements use it, and for a rigged model its clip names |
| Card thumbnails | ✅ | `list_assets` (state only) | An agent can see how far a thumbnail got, not look at it. Add an image route if that is ever wanted |
| Built-in items the browser offers besides files | ✅ | `list_folders`, `place_asset`, `add_light`, `add_player_start` | general › lighting, shapes and tools. Each drops through the tool for what it makes. The shapes are Cube, Cylinder, Pyramid, Sphere and Tile — a flat slab for floors, rugs and holes, which alone lands with Collides unticked |
| Import other formats — FBX, static glTF, sprite sheets (sounds: §6.3) | ❌ | ❌ | [REQUIREMENTS §8](REQUIREMENTS.md#8-asset-pipeline) |

## 4. Placing and editing

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Place an asset on a tile | ✅ | `place_asset` | |
| Add a light | ✅ | `add_light` | |
| Mark where a player starts | ✅ | `add_player_start` | Dragged from general › tools. Each start is for one of the four players, and a dropped one takes the lowest player without a start yet; the viewport draws it as a person-height column in that player's colour |
| Give a start to another player | ✅ | `set_property` (`player`) | The panel's Player row steps 1–4; a value outside that is clamped |
| Define the characters a player can play as | 🚧 | `list_characters` | Hand-written in `content/data/characters.data.json` ([project-format.md §8.1](project-format.md#81-what-the-editor-reads-today)): each a name, a model, a move speed and health. Read when the project opens, on a rescan, and on every Play; problems with the file are logged and reported by `list_characters`. Edited in the editor once the data-editing panel exists |
| Give a start a default character | ✅ | `set_character`, `list_player_starts` | A Character row, last in a start's properties, steps through None and every character, as one undoable edit. The character's model stands on the start in the viewport, as tall as a player and facing +X. Saved with the start; one naming a character the table no longer has is kept |
| Read every player start | ✅ | `list_player_starts` | With the player each is for, its default character, and how many players a session holds |
| Read every placement | ✅ | `list_placements` | |
| Read every light | ✅ | `list_lights` | |
| Move or turn an entry | ✅ | `set_property`, `translate` | Absolute or by a delta |
| Aim, dim, tint, and set a light's reach | ✅ | `set_property` | |
| Resize a prop | ✅ | `set_property` (`scale`), `list_placements` | A Scale slider in a prop's properties, 1 in its middle and logarithmic either side; pressing it jumps there and dragging follows the pointer, as one undoable edit, and its step buttons move between stops a quarter-doubling apart. One uniform size, held to 0.125–8. Picking and playtest collision use the scaled box. Saved with the prop, and only when it is not 1 |
| Choose whether a prop blocks players | ✅ | `set_property` (`collides`), `list_placements` | A Collides checkbox, last in a prop's properties; a click anywhere on the row flips it, as one undoable edit. Props collide by default. One that does not has its footprint drawn faded in the viewport, and it is saved with the prop |
| Choose which animation clip a rigged prop plays | ✅ | `set_animation`, `list_placements` | An Animation row, below a rigged prop's properties, names the clip; its step buttons move to the previous or next, wrapping round, as one undoable edit, and the prop crossfades to the new clip over a fifth of a second — `set_animation` fades the same way. A prop naming none plays the model's first clip, so a dropped model moves at once. Every placed clip loops on the viewport's own clock, in edit mode and in a playtest alike; it is presentation and never reaches the simulation. Saved with the prop |
| Give a prop intelligence — make it an actor | ✅ | `set_behavior`, `list_placements` | A Behavior row, below a prop's properties, steps through None, the built-in behaviors and the project's own, as one undoable edit. A prop with a behavior is an actor in a playtest ([actors.md](../game/actors.md)) and is no longer a collision box for players. Its footprint is outlined in its faction's colour with a tick the way it faces. Saved with the prop; one naming a behavior the project no longer has is kept, and plays as idle |
| Choose which side an actor is on | ✅ | `set_behavior` (`faction`), `list_placements` | A Faction row — Hostile, Neutral, Friendly — shown once the prop has a behavior. Hostile and friendly actors take players as targets — or, with a behavior whose `targets` is `opponents` (the Defender preset), each other: friendly against hostile, and hostile against players and friendly; a neutral one takes nobody |
| Lay out a patrol route | ✅ | `add_waypoint`, `list_waypoints`, `set_property` (`route`, `order`), `translate`, `delete`, `select` | The Waypoint card in general › tools. Each waypoint belongs to one of nine routes and has a place in it, edited as its Route and Order rows; one dropped while a waypoint is selected joins that route after its last. The viewport draws a knee-high post and joins each route's waypoints in walking order, in the route's colour. Saved as `entity:waypoint` entities |
| Try out particle effects with an emitter | ✅ | `add_emitter`, `set_effect`, `list_emitters`, `set_property` (`interval`, `particles`, `spread`, … `flash_time`), `translate`, `delete`, `select` | The Particle Emitter card in general › effects. Dropped at chest height, it throws a burst every interval and flashes a light over the meshes near it — live in the viewport while the level is edited, and in a playtest. Its panel opens with an Effect row that starts it from any built-in burst (muzzle flash, muzzle sparks, wall sparks, grit, hit spray, fireball, embers, smoke, smoke plume), marked `(edited)` once changed; under it, rows for its direction, interval, particle count, cone, speed, life, size, start and end colour and how much each hides, gravity, drag, streak, spin, Textured and Lit as toggles — a puff of noise rather than a smooth disc, and coloured by the scene's lights rather than its own — and its flash's brightness, reach and length. The panel scrolls to hold them. Every change is one undoable edit, and the viewport draws it as a violet box. Saved as `entity:fx_emitter` entities; presentation only, never simulated ([fx.md](../engine/fx.md)) |
| Stand 2D art in a level with a sprite billboard | ✅ | `add_sprite`, `set_sheet`, `list_sprites`, `set_property` (`height`, `columns`, `rows`, `frames`, `fps`), `translate`, `delete`, `select` | The Sprite Billboard card in general › sprites. Dropped feet-on-the-floor, it is an upright quad facing the camera playing one frame of a sprite sheet at a time — drawn in the same depth-buffered pass the meshes are, with its empty texels cut out, so what is in front of it hides it and what is behind it does not ([sprites.md](../engine/sprites.md)). Its panel opens with a Sheet row that steps through every `.png` and `.tga` under the project's assets, marked `(missing)` for one the project no longer holds; under it, rows for where it stands, how tall, and the grid and speed its sheet plays at. Its width is not a row: it follows from the frame's own shape, so a sheet is never stretched. Every change is one undoable edit, and the viewport draws it as a teal box. Saved as `entity:sprite_billboard` entities; presentation only, never simulated |
| Paint the ground — floors, roads, sand, water, holes | ✅ | `paint_ground`, `get_ground`, `set_tool` (`tile`) | The cards in general › ground — seven terrains and Erase — take the brush, and with the Tile tool out a left drag paints under a square brush 1 to 9 tiles across (`[` and `]` resize it). Painted areas are autotiled: rounded ends and edges, filled bends, a later terrain drawn over an earlier one where they meet ([ground.md](../engine/ground.md)). A stroke is one undoable edit, and an agent fills a rectangle per call rather than dragging a brush. Saved as the level's tile layer ([project-format.md §4.1](project-format.md#41-what-the-editor-writes-today)); presentation only — no terrain blocks anybody yet |
| Fire an effect on request, and see what effects are doing | ❌ | `play_effect`, `get_effects` | Agent-only for now: plays a preset burst, a whole combat effect (a muzzle flash, a hit on a wall or a body, a blast) or a placed emitter's own burst once, into whatever the viewport is drawing — the editor's effects while editing, the playtest's while playing, where a paused one holds it. A blast also leaves a cloud of volumetric smoke standing, so `play_effect {"effect": "blast"}` is how smoke is put in front of the camera. `get_effects` reports particles, clouds and flashes alive, each emitter's bursts, and effects played on request, in either mode. Nothing is recorded or saved. No menu or key fires one yet |
| Choose the route an actor patrols | ✅ | `set_behavior` (`route`), `list_placements`, `list_waypoints` | A Route row — None and every route in use — shown once the prop has a behavior, as one undoable edit. What its behavior's `patrol` state walks; `list_waypoints` names who patrols each route. Saved with the prop |
| Define the behaviors props can run | 🚧 | `list_behaviors` | Ten built in; the project's own hand-written in `content/data/behaviors.data.json` ([project-format.md §8.2](project-format.md#82-the-behaviors-table)), a row with a built-in's id replacing it. Read when the project opens, on a rescan, and on every Play; problems are logged and reported by `list_behaviors`. Edited in the editor once the data-editing panel exists |
| Define the enemy archetypes a horde spawns from | 🚧 | `list_enemies` | Hand-written in `content/data/enemies.data.json` ([project-format.md §8.3](project-format.md#83-the-enemies-table)): each a model, health, body and behavior. Read when the project opens, on a rescan, and on every Play; problems are logged and reported by `list_enemies`, which also says whether each archetype's behavior resolves. Nothing spawns them until the director exists; edited in the editor once the data-editing panel does |
| Select, and clear the selection | ✅ | `select`, `get_selection` | |
| Delete a placement, a light, a player start, a waypoint, an emitter or a billboard | ✅ | `delete`, `run_command` (`delete_selection`) | Backspace or Delete removes what is selected, and so does Edit > Delete; the tool takes any entry by index. One undoable edit either way — the entry travels in the action, so undo puts back the one that was there |
| Duplicate an entry | ❌ | ❌ | The action kinds a removal needed are built now; a duplicate is an insert of a copy at the end |
| Multi-select | ❌ | ❌ | `EditorSelection` holds one entry by design |
| Copy and paste | ❌ | ❌ | Listed in the menu, disabled |

## 5. History

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Every edit is undoable | ✅ | — | Every write tool records an `EditorAction`, in the same list the panels write to |
| Undo and redo | ✅ | `undo`, `redo` | |
| Read the history | ✅ | `get_history` | The actions and the cursor |
| History survives a rescan | ❌ | — | Dropped on purpose: a rescan renumbers what the actions name |

## 6. View and chrome

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Read the camera, zoom, grid, projection, shading, and hovered tile | ✅ | `get_state` | Mirrored into shell state once a tick — `editor-view-state.h`; `camera.projection` names the projection and `camera.shading` the look |
| Reset the view, zoom, toggle the grid | ✅ | `run_command` | Reset keeps the projection: that is the project's setting, not the camera's position |
| Switch between the dimetric and isometric projections | ✅ | `run_command` (`set_view_dimetric`, `set_view_isometric`) | Writes the choice into `project.json`; refused with no project open |
| Switch between smooth and cel shading | ✅ | `run_command` (`set_shading_smooth`, `set_shading_cel`) | Takes effect on the next frame and writes the choice into `project.json`; refused with no project open. Cel bands the light on every backend with a mesh pipeline, and outlines on those with an outline pipeline too — Metal, DX12, OpenGL |
| Pan and zoom by dragging | ✅ | — gesture only | An agent uses the camera commands |
| Choose the active tool | ✅ | `set_tool`, `get_state` | Only Select does anything today |
| Read every menu command and its state | ✅ | `list_commands` | Says which are built and which would work right now |
| Run a menu command | ✅ | `run_command` | A command the menu greys out is refused, by the same rule |
| Quit | ✅ | `run_command` (`exit`) | |
| Toolbar status line | ✅ | ❌ | An agent reads the state the line is derived from, not the line |
| See where actors can walk | ✅ | `get_navigation`, `run_command` (`toggle_navigation`), `get_state` (`camera.show_navigation`) | View › Navigation Overlay shades every cell an actor cannot use — solid, too narrow for an actor a player's width, or walled off from every player start — from the grid a playtest would build, and the status line names actors no path joins to a start. `get_navigation` reports the counts and those actors |
| Ask how an actor would get somewhere | — | `find_path` | The route the game's A* and smoothing would plan between two points for an actor of any radius, with its length. Nothing in the interface asks this; the overlays show the answers the game actually reaches |
| Dockable panels, workspaces | ❌ | ❌ | [REQUIREMENTS §3](REQUIREMENTS.md#3-editor-shell) |

## 6.1 Playtest

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Play the open level, and stop | ✅ | `start_playtest`, `stop_playtest`, `run_command` (`playtest`) | The toolbar's Play button, Level › Play Level, or F5; Esc stops too. One player, spawning on the level's first start for player 1, or under the camera without one |
| See the running game | ✅ | `get_playtest`, `get_state` (`playtest`) | Tick, where each player is, who they play as and their health, the latest tick hash, dropped ticks. The toolbar status line shows the tick while playing |
| Pick a character before playing | ✅ | `start_playtest` (`character`), `get_playtest` (`mode`: `choosing`), `stop_playtest` | With two or more characters, Play opens a selector over the viewport: a card per character with its model's picture, name, speed and health, opening on the one player 1's start names, or the first. A click or Enter plays as it; Esc, F5 or a click off the panel cancels. The pick is simulation input — the player's speed and health come from it, and the replay records it. `start_playtest` skips the selector, playing as the character it is given or the one the selector would open on |
| Control player 1 | ✅ | `send_input` | WASD or the arrows move relative to the camera (W is up the screen in either projection), the cursor aims, the left button fires. Any pad the desktop supports plays twin-stick — left stick or d-pad moves, right stick aims (the cursor aims while it rests), right trigger or shoulder fires, Start pauses — and the d-pad, South and East drive the character selector. Keys and pad are remapped on Edit › Controls (§6.2) or by hand in `input-bindings.json` in the user's application data ([input.md](../engine/input.md#5-the-bindings-file)). While a pad is what the player last touched, a resting right stick keeps the aim rather than handing it to the cursor. `send_input` queues exact input for a run of ticks, in place of the keyboard, so a playthrough can be scripted and checked by its hashes. Its stick is in world axes, not the camera's, so a script means the same run under either projection |
| Props stop the player | ✅ | — | Every prop whose Collides box is ticked is a solid box to the playtest: the one the viewport outlines, so a turned prop blocks the box around it rather than its exact shape. The player stops against it and slides along it |
| The level is untouched by playing it | ✅ | — | The game runs from a copy; every edit — browser drops, picks, the panel, Delete, undo — is ignored while playing, and the API's edit tools are refused |
| Every playtest records a replay | 🚧 | `stop_playtest` | Written to `data/playtests/<level>.replay` when the playtest stops. Nothing plays one back in the editor yet |
| Pause and single-step | ✅ | `step_playtest`, `run_command` (`pause_playtest`, `step_tick`), `get_playtest` (`paused`) | F6 or Level › Pause Playtest pauses and resumes; F7 runs exactly one tick on the input held or queued. `step_playtest` runs up to 3,600 ticks and leaves the playtest paused, so an agent can land on an exact tick |
| Speed multipliers | ❌ | ❌ | [REQUIREMENTS §7](REQUIREMENTS.md#7-playtest) |
| Actors play their behaviors | ✅ | `get_playtest` (`actors`) | Every prop with a behavior perceives the players — and other actors, if its behavior targets opponents — plans a path round the props, or walks down a player's flow field when it is chasing them, turns and moves on the tick, and is drawn where the game has it — turned to face its way, playing its state's clip or its walk and idle clips. `get_playtest` reports each actor's position, facing, behavior, state, faction, target (a player, or an actor by id) and path |
| Actors attack, and players are hurt | ✅ | `get_playtest` (`players`, `actors`, `projectiles`, `hazards`, `run_over`) | Behaviors bite, rush, fire volleys, lob hazard pools and burst ([actors.md §4](../game/actors.md#4-attacks-damage-and-death)); players lose health segments, go down and are revived by a teammate or put out; actors die. Projectiles are drawn as small orange boxes and pools as green squares; the status line gives player 1's health, and says when they are down or the run is over; the AI Overlay labels each actor with its health |
| See shots, hits and blasts | ✅ | `get_playtest` (`effects`) | Every shot fired throws a muzzle flash, every shot that lands throws sparks off a wall or a dark spray off whoever it hit, and every blast throws a fireball, embers, smoke and a cloud of volumetric smoke that stands for a few seconds after them; each flashes a light over the meshes near it for a moment, in whatever of the scene's eight light slots the level's own lights leave. Particles are hidden behind props and fade into what they touch, and a cloud stops where the scene's depth says a surface is, so it wraps a crate and fills a doorway. Effects run on the frame's clock and freeze while paused; F7 moves them on a tick. `get_playtest`'s `effects` gives the particles, clouds and flashes live now and how many `shot_fired`, `shot_hit_body`, `shot_hit_wall` and `blast` cues the run has played. Built-in effects only, and nothing the simulation reads ([fx.md](../engine/fx.md)) |
| Multi-player preview with stand-ins | ✅ | `start_playtest` (`stand_ins`), `run_command` (`play_solo`, `play_with_1_stand_in` … `play_with_3_stand_ins`), `get_playtest` (`stand_ins`, `players[].stand_in`) | Level › Play Solo or Play with 1–3 Stand-ins sets how many stand-in players the next playtest adds, each on its player's start or beside player 1; the game's stand-in plays them — reviving teammates, backing away from hostiles, keeping up. Their input is recorded in the replay like anyone's. Scaling curves wait on the director |
| Couch co-op: players 2–4 on pads | ✅ | `get_playtest` (`players[].pad`) | A pad takes the next free seat the first time it is touched — seat 1 is player 1's, alongside the keyboard — and Play adds a player for every seated pad, played by it in place of a stand-in; stand-ins fill the other seats up to the Level menu's number. A pad unplugged mid-run hands its player to the stand-in, and one touched mid-run takes over a stand-in's seat. The status line names the pad players. An agent cannot hold a pad; `send_input` drives player 1 |
| Hear shots, hits and blasts | ✅ | `get_playtest` (`effects.sounds`) | Every shot fired cracks, a shot that lands thuds, one a wall stops ticks, and a blast booms and ducks the music under it, each from where it happened: quieter the further it is from player 1, and panned to the side of the screen it is on as the camera turns. Of each kind a tick leaves, only the four nearest are heard, so a horde firing at once does not drown the rest. The sounds are synthesised stand-ins unless the project gives a slot its own file (§6.3). Presentation only, from the combat cues ([audio.md](../engine/audio.md)); `effects.sounds` counts those sent to be heard. Volumes, and the project's own sound files, are §6.3 |
| Feel the fight through the pad | ✅ | — | Player 1's pad rumbles: a kick through the trigger for their own shot, a thump for a blast that fades with distance, a jolt for a hit taken, harder the more of the bar it took. Trigger motors on Xbox pads; body motors on every pad that has them. Presentation only, from the combat cues ([input.md](../engine/input.md)); nothing an agent can read |
| See what actors think | ✅ | `get_playtest` (`actors`), `run_command` (`toggle_ai_overlay`), `get_state` (`camera.show_ai`) | View › AI Overlay draws, while playing, each actor's view cone, the rest of its path, a line to the target it sees, and its state's name. `get_playtest` reports the same per actor |
| Other debug overlays during play | ❌ | ❌ | Collision, flow fields, budgets, per-phase timing |

## 6.2 Controls

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Remap keys and pad controls | ✅ | `get_controls`, `set_controls`, `run_command` (`controls`) | Edit › Controls opens a screen over the viewport listing every action with its keys and its pad controls, the pad's labelled as the pad in use prints them. Arrows or the d-pad choose a row; Enter, a click or the pad's confirm button listens, and the next key or pad control — button, stick direction or trigger — replaces that action's controls on that device. Delete clears a row, R resets every action to the defaults keeping the deadzones, Esc closes. Saved at once to `input-bindings.json` in the user's application data ([input.md §5](../engine/input.md#5-the-bindings-file)); `set_controls` also sets the deadzones. The user's, not the project's, and not undoable. Refused while playing |

## 6.3 Sound

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Set the volume | ✅ | `get_sound`, `set_volume`, `run_command` (`sound`) | Edit › Sound opens a screen over the viewport: Master, Effects, Music and Interface as bars — left and right turn them in 5% steps, a click on a bar sets it there — and Mute (Enter, or M from any volume row). A volume is a slider's position; the gain is its square, so halfway sounds like half. Heard at once, allowed while playing, and saved to `audio-volumes.json` in the user's application data beside their controls, so it follows them across projects. Not part of the level's undo history ([audio.md §8](../engine/audio.md#8-volume-settings)) |
| Give the game's sounds the project's own files | ✅ | `get_sound`, `set_sound`, `play_sound` | The Sound screen's Project sounds rows list each of the game's sounds — Shot fired, Shot hits someone, Shot hits a wall, Blast — and the file it plays. Left and right step through Built-in and every `.wav` and `.ogg` under `assets/`; Enter plays it as the game will; Delete puts the built-in back. Written to `content/data/sounds.data.json` ([project-format §8.4](project-format.md#84-the-sounds-table)) and heard from the next sound played. A file that is gone or will not decode plays the built-in sound, and says so in the log and in `get_sound`'s `problems` |
| Import a sound file | ✅ | `import_sound`, `run_command` (`import_sound`) | File › Import Sound — or I on the Sound screen, which also puts it in the highlighted slot — opens the system file picker for a `.wav` or `.ogg`. The file must decode, or it is refused before anything is copied; it is copied into `assets/sounds/` under its own name, or with `-2`, `-3` … when that is taken, never over a file the project has. One already under `assets/` is used where it is. A file dropped into `assets/` by hand is listed the next time the project is opened or rescanned |
| Loudness normalisation, format conversion, sound banks | ❌ | ❌ | [REQUIREMENTS §8](REQUIREMENTS.md#8-asset-pipeline): a file plays as it was recorded |

## 7. Not built at all

Nothing below has an interface, so nothing below has an API. They are here
so that the register is a complete map rather than a list of what happens to
exist, and so that whoever builds one knows the API is part of building it.

| Capability | Specified in |
|---|---|
| Height painting, flood fill and per-project tilesets | [§4.1](REQUIREMENTS.md#41-the-grid) — tile painting and autotiling are built (§4 above), with the editor's own terrains |
| Entity placement and property blocks, beyond the player start and the waypoint | [§4.2](REQUIREMENTS.md#42-props-and-entities) — the player start and the waypoint are the two definitions built, each with its properties; a general entity needs a definition schema to render a property block from |
| Flow-field overlay | [§4.3](REQUIREMENTS.md#43-navigation-and-flow) — reachability is drawn by the Navigation Overlay. The game builds a flow field per player now ([spatial.md §5](../engine/spatial.md#5-flow-fields-and-neighbours)); nothing in the editor draws one yet |
| Encounter and wave authoring | [§5](REQUIREMENTS.md#5-encounter-and-wave-authoring) |
| Data-table editing | [§6](REQUIREMENTS.md#6-data-editing) |
| Hot-reload of data files | [§3](REQUIREMENTS.md#3-editor-shell) |
| Autosave and crash recovery | [§9](REQUIREMENTS.md#9-non-functional-requirements) |

---

## 8. Keeping this current

Update a row in the change that moves it, not afterwards. A change that
adds an editor capability touches three things:

1. The code.
2. The agent API — [agent-api.md §6](agent-api.md#6-adding-a-tool--the-rule)
   is the checklist, and most of it is enforced by `static_assert`s and by
   `test_agent_tool_info.cpp`.
3. This table.

What the compiler cannot check is the first column: nothing fails a build
because a feature shipped without a row here. That is what makes this a
register somebody has to keep rather than a report something generates —
and why a change that leaves it stale is an incomplete change.
