# Simplish — Editor Capabilities

**Parent document:** [Editor REQUIREMENTS](REQUIREMENTS.md)
**Version:** 1.0
**Status:** Living register — update it in the change that moves a row
**Last Updated:** 2026-09-10 (playtest)

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
| Scan `.obj` files under `assets/` | ✅ | `list_assets`, `rescan_assets` | |
| Browse them by folder | ✅ | `list_folders` | Includes the built-in section beside the project's own folders |
| Read one asset's state | ✅ | `get_asset` | Mesh loaded, load failed, thumbnail state, measured bounds, how many placements use it |
| Card thumbnails | ✅ | `list_assets` (state only) | An agent can see how far a thumbnail got, not look at it. Add an image route if that is ever wanted |
| Built-in items the browser offers besides files | ✅ | `list_folders`, `place_asset`, `add_light`, `add_player_start` | general › lighting, shapes and tools. Each drops through the tool for what it makes |
| Import anything but `.obj` | ❌ | ❌ | [REQUIREMENTS §8](REQUIREMENTS.md#8-asset-pipeline) |

## 4. Placing and editing

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Place an asset on a tile | ✅ | `place_asset` | |
| Add a light | ✅ | `add_light` | |
| Mark where a player starts | ✅ | `add_player_start` | Dragged from general › tools. Each start is for one of the four players, and a dropped one takes the lowest player without a start yet; the viewport draws it as a person-height column in that player's colour |
| Give a start to another player | ✅ | `set_property` (`player`) | The panel's Player row steps 1–4; a value outside that is clamped |
| Read every player start | ✅ | `list_player_starts` | With the player each is for, and how many players a session holds |
| Read every placement | ✅ | `list_placements` | |
| Read every light | ✅ | `list_lights` | |
| Move or turn an entry | ✅ | `set_property`, `translate` | Absolute or by a delta |
| Aim, dim, tint, and set a light's reach | ✅ | `set_property` | |
| Choose whether a prop blocks players | ✅ | `set_property` (`collides`), `list_placements` | A Collides checkbox, last in a prop's properties; a click anywhere on the row flips it, as one undoable edit. Props collide by default. One that does not has its footprint drawn faded in the viewport, and it is saved with the prop |
| Select, and clear the selection | ✅ | `select`, `get_selection` | |
| Delete a placement, a light or a player start | ✅ | `delete`, `run_command` (`delete_selection`) | Backspace or Delete removes what is selected, and so does Edit > Delete; the tool takes any entry by index. One undoable edit either way — the entry travels in the action, so undo puts back the one that was there |
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
| Dockable panels, workspaces | ❌ | ❌ | [REQUIREMENTS §3](REQUIREMENTS.md#3-editor-shell) |

## 6.1 Playtest

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Play the open level, and stop | ✅ | `start_playtest`, `stop_playtest`, `run_command` (`playtest`) | The toolbar's Play button, Level › Play Level, or F5; Esc stops too. One player, spawning on the level's first start for player 1, or under the camera without one |
| See the running game | ✅ | `get_playtest`, `get_state` (`playtest`) | Tick, where each player is, the latest tick hash, dropped ticks. The toolbar status line shows the tick while playing |
| Control player 1 | ✅ | `send_input` | WASD or the arrows move relative to the camera (W is up the screen in either projection), the cursor aims, the left button fires (recorded; nothing fires yet). `send_input` queues exact input for a run of ticks, in place of the keyboard, so a playthrough can be scripted and checked by its hashes. Its stick is in world axes, not the camera's, so a script means the same run under either projection |
| Props stop the player | ✅ | — | Every prop whose Collides box is ticked is a solid box to the playtest: the one the viewport outlines, so a turned prop blocks the box around it rather than its exact shape. The player stops against it and slides along it |
| The level is untouched by playing it | ✅ | — | The game runs from a copy; every edit — browser drops, picks, the panel, Delete, undo — is ignored while playing, and the API's edit tools are refused |
| Every playtest records a replay | 🚧 | `stop_playtest` | Written to `data/playtests/<level>.replay` when the playtest stops. Nothing plays one back in the editor yet |
| Pause, single-step, speed multipliers | ❌ | ❌ | [REQUIREMENTS §7](REQUIREMENTS.md#7-playtest). The session steps one tick at a time already; the controls are what is missing |
| Multi-player preview with stand-ins | ❌ | ❌ | Only player 1 spawns; the other starts are shown but empty |
| Debug overlays during play | ❌ | ❌ | Collision, flow fields, budgets, per-phase timing |

## 7. Not built at all

Nothing below has an interface, so nothing below has an API. They are here
so that the register is a complete map rather than a list of what happens to
exist, and so that whoever builds one knows the API is part of building it.

| Capability | Specified in |
|---|---|
| Tile and height painting | [§4.1](REQUIREMENTS.md#41-the-grid) |
| Entity placement and property blocks, beyond the player start | [§4.2](REQUIREMENTS.md#42-props-and-entities) — the player start is the one definition built, with its `player` property; a general entity needs a definition schema to render a property block from |
| Flow-field and reachability overlays | [§4.3](REQUIREMENTS.md#43-navigation-and-flow) |
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
