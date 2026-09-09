# Simplish — Editor Capabilities

**Parent document:** [Editor REQUIREMENTS](REQUIREMENTS.md)
**Version:** 1.0
**Status:** Living register — update it in the change that moves a row
**Last Updated:** 2026-09-08

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
| Save a level | ❌ | ❌ | Nothing is persisted yet — [REQUIREMENTS §4.4](REQUIREMENTS.md#44-level-format) |

## 3. Assets

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Scan `.obj` files under `assets/` | ✅ | `list_assets`, `rescan_assets` | |
| Browse them by folder | ✅ | `list_folders` | Includes the built-in section beside the project's own folders |
| Read one asset's state | ✅ | `get_asset` | Mesh loaded, load failed, thumbnail state, measured bounds, how many placements use it |
| Card thumbnails | ✅ | `list_assets` (state only) | An agent can see how far a thumbnail got, not look at it. Add an image route if that is ever wanted |
| Built-in items the browser offers besides files | ✅ | `list_folders`, `place_asset`, `add_light` | Both drop through the same tools a scanned model does |
| Import anything but `.obj` | ❌ | ❌ | [REQUIREMENTS §8](REQUIREMENTS.md#8-asset-pipeline) |

## 4. Placing and editing

| Capability | In the editor | Agent | Notes |
|---|---|---|---|
| Place an asset on a tile | ✅ | `place_asset` | |
| Add a light | ✅ | `add_light` | |
| Read every placement | ✅ | `list_placements` | |
| Read every light | ✅ | `list_lights` | |
| Move or turn an entry | ✅ | `set_property`, `translate` | Absolute or by a delta |
| Aim, dim, tint, and set a light's reach | ✅ | `set_property` | |
| Select, and clear the selection | ✅ | `select`, `get_selection` | |
| **Delete a placement or a light** | ❌ | ❌ | **The largest gap.** There is no `EditorActionKind` for a removal, so neither the interface nor the API can undo one. Adding it means a new action kind and its inverse, and then a `delete` tool |
| Duplicate an entry | ❌ | ❌ | Follows delete: both want the same new action kinds |
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
| Read the camera, zoom, grid, projection, and hovered tile | ✅ | `get_state` | Mirrored into shell state once a tick — `editor-view-state.h`; `camera.projection` names the projection |
| Reset the view, zoom, toggle the grid | ✅ | `run_command` | Reset keeps the projection: that is the project's setting, not the camera's position |
| Switch between the dimetric and isometric projections | ✅ | `run_command` (`set_view_dimetric`, `set_view_isometric`) | Writes the choice into `project.json`; refused with no project open |
| Pan and zoom by dragging | ✅ | — gesture only | An agent uses the camera commands |
| Choose the active tool | ✅ | `set_tool`, `get_state` | Only Select does anything today |
| Read every menu command and its state | ✅ | `list_commands` | Says which are built and which would work right now |
| Run a menu command | ✅ | `run_command` | A command the menu greys out is refused, by the same rule |
| Quit | ✅ | `run_command` (`exit`) | |
| Toolbar status line | ✅ | ❌ | An agent reads the state the line is derived from, not the line |
| Dockable panels, workspaces | ❌ | ❌ | [REQUIREMENTS §3](REQUIREMENTS.md#3-editor-shell) |

## 7. Not built at all

Nothing below has an interface, so nothing below has an API. They are here
so that the register is a complete map rather than a list of what happens to
exist, and so that whoever builds one knows the API is part of building it.

| Capability | Specified in |
|---|---|
| Tile and height painting | [§4.1](REQUIREMENTS.md#41-the-grid) |
| Level save and load | [§4.4](REQUIREMENTS.md#44-level-format) |
| Entity placement and property blocks | [§4.2](REQUIREMENTS.md#42-props-and-entities) |
| Flow-field and reachability overlays | [§4.3](REQUIREMENTS.md#43-navigation-and-flow) |
| Encounter and wave authoring | [§5](REQUIREMENTS.md#5-encounter-and-wave-authoring) |
| Data-table editing | [§6](REQUIREMENTS.md#6-data-editing) |
| Playtest | [§7](REQUIREMENTS.md#7-playtest) |
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
