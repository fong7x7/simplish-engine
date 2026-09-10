# Simplish — Editor Agent API

**Parent document:** [Editor REQUIREMENTS](REQUIREMENTS.md)
**Version:** 1.0
**Status:** Built — 23 tools, HTTP transport, MCP bridge
**Last Updated:** 2026-09-09

The editor answers to an agent the same way it answers to a person: through
one enumerated set of tools over the state the panels already show. This
document is the contract, and §6 is the rule that keeps it honest — a tool
added to the editor is not finished until it is here.

---

## 1. Why this exists

Level authoring is a loop of *look, judge, nudge*. A person does it with a
pointer; an agent has to do it with a vocabulary. Everything else follows
from wanting that vocabulary to be exactly the editor's, rather than a
second, smaller editor written for machines:

- **One document, one history.** An agent's edit is an `EditorAction` in the
  same undo list as a drag on the properties panel. Ctrl+Z reverts what the
  agent did, and the agent's `undo` reverts what the person did.
- **One frame.** Requests are drained on the editor's own tick, on the main
  thread, between the input the window delivered and the chrome rebuilt from
  the result. There is no lock anywhere, because there is no second thread.
- **One description.** The tool table in
  [`agent-tool-info.h`](../../src/editor/agent/include/editor/agent/agent-tool-info.h)
  is the only place a tool is described. The HTTP manifest is generated from
  it, and the MCP bridge builds its tool list by reading that manifest, so
  neither can fall behind the editor.

---

## 2. Shape

```
  agent (Claude, or anything else)
        │  MCP over stdio
        ▼
  scripts/simplish-editor-mcp.py        ← holds no tool list of its own
        │  HTTP/1.1 on 127.0.0.1
        ▼
  src/platform/agent/     LocalAgentServer — sockets, polled once a tick
        │  AgentHttpRequest
        ▼
  src/editor/agent/       EditorAgentService — routes; runAgentTool dispatches
        │  AgentResult { status, json, host, changed }
        ▼
  EditorShellState        the document, the history, the selection, the view
```

Three properties are worth naming, because they are what the split buys:

**The tool surface is a pure function.** `runAgentTool(state, tool, params)`
takes shell state and gives back a result. It opens nothing, draws nothing,
and holds no pointer to the editor — which is why every tool in it is tested
against a bare `EditorShellState` with no window and no GPU.

**What it cannot do, it hands back.** Three things need the running editor: a
camera move (the camera lives in a widget), opening a project (the disk and
the window title), and a rescan (GPU textures). A tool that needs one leaves
an `AgentHostRequest` in its result and the service carries it out on the
same tick. See [`agent-host-request.h`](../../src/editor/agent/include/editor/agent/agent-host-request.h).

**The shell does not know an agent exists.** `SimplishEditor` grew one
generic extension point — `setStateHook`, a callback run once a tick with
mutable shell state — and nothing that names agents, sockets, or tools. The
hook returns whether it changed anything, which matters: rebuilding the
properties panel on every tick would tear a drag in progress out from under
the pointer.

---

## 3. Running it

The editor opens no port unless it is told to. A local port that edits
someone's project is not a thing to switch on for everybody who launches the
editor.

```bash
SIMPLISH_AGENT_PORT=default ./scripts/editor.sh ~/projects/my-level
```

| Value | Effect |
|---|---|
| unset or empty | No port. The editor runs exactly as it always did |
| `default` | Port 8787 (`EDITOR_AGENT_DEFAULT_PORT`) |
| a number | That port |
| anything else | No port — a typo must not quietly open a different one |

`SIMPLISH_AGENT_HOST` and `SIMPLISH_AGENT_PORT` also tell the MCP bridge
where to look. The server binds `127.0.0.1` and nothing else, so nothing off
this machine can reach it; that is the whole of its access control, which is
why it is opt-in.

### As an MCP server

The repository ships an [`.mcp.json`](../../.mcp.json), so a Claude Code
session started in this checkout finds the bridge already configured. To
register it elsewhere:

```bash
claude mcp add simplish-editor -- python3 /path/to/simplish2d/scripts/simplish-editor-mcp.py
```

The bridge is stdlib-only Python 3 and depends on nothing. It fetches
`GET /tools` on every `tools/list`, so **a tool added to the editor appears
in MCP with no change to the bridge** — start the editor, and it is there.

---

## 4. HTTP endpoints

| Method | Path | Body | Answers |
|---|---|---|---|
| `GET` | `/` | — | `describe`: the manifest and the current state |
| `GET` | `/tools` | — | The manifest alone |
| `GET` | `/state` | — | `get_state` |
| `POST` | `/call` | `{"tool": "...", "params": {...}}` | That tool |
| `POST` | `/tools/<name>` | the params alone | That tool |

Every response is JSON. A `POST` answers with an envelope:

```json
{
  "status": "ok",
  "changed": true,
  "result": { "index": 0, "position": { "x": 15.0, "y": 5.0, "z": 3.0 } }
}
```

`status` is one of `ok`, `unknown_tool`, `bad_params`, `not_found`, or
`unavailable` — the distinctions a caller can act on: a name to stop using,
a parameter to fix, an index to re-read, and a state to change first. A
failure puts a `message` in `result` written in terms of the call that was
made. `changed` says whether the editor's state moved, which is false for
every read and for a write that turned out to change nothing.

The HTTP status is 200 for anything that routed, whatever the tool made of
it: a tool that refuses says so in its own payload, where a caller reading
the JSON will see it. Only an unrouted path is a 404, and it answers with
the list of paths that do exist.

---

## 5. The tools

Twenty-eight, in three groups. `GET /tools` is authoritative and carries
each one's parameters; this table is the map.

### Reading

| Tool | Answers |
|---|---|
| `describe` | What this editor is and every tool it offers. Start here |
| `get_state` | Project, active tool, camera, selection, counts, undo/redo |
| `list_assets` | Every scanned asset: index, name, paths, bounds, whether its mesh is loaded, whether loading failed, how far its thumbnail got, how many placements instance it |
| `get_asset` | One of those, by index or by name |
| `list_folders` | The browser's folder tree, the built-in general section included |
| `list_placements` | Every placement: index, asset, position, rotation |
| `list_lights` | Every light: kind, position, direction, colour, intensity, range |
| `list_player_starts` | Every player start: the player it is for, its position, and how many players a session holds |
| `get_selection` | What the properties panel is editing, and the rows it lists |
| `get_history` | Every edit this session, and how many are applied |
| `get_level` | The level file behind the document: its id and path, whether one is on disk, whether it could be read, and whether the document has unwritten changes |
| `list_levels` | Every level the open project holds, which one is being edited, and whether each has a file yet |
| `list_commands` | Every menu command, its label, its shortcut, whether it is built, and whether it would work right now |

### Editing

| Tool | Does |
|---|---|
| `place_asset` | Places an asset on a tile and selects it, as a browser drag would |
| `add_light` | Adds a directional or point light and selects it |
| `add_player_start` | Marks where a player spawns and selects it, for a named player or the lowest one with no start yet |
| `set_property` | Writes one property to an absolute value |
| `translate` | Moves a placement, a light or a player start by a delta in tiles |
| `delete` | Removes a placement, a light or a player start, as the Delete key does |
| `select` | Selects a placement, a light or a player start, or clears the selection |
| `set_tool` | Chooses the active toolbar tool |
| `undo` / `redo` | Walks the same history the Edit menu walks |

### Driving the editor

| Tool | Does |
|---|---|
| `run_command` | Runs a menu command — camera, grid, close project, quit |
| `open_project` | Opens the project in a directory |
| `rescan_assets` | Rescans from disk, which drops the level and its history |
| `create_level` | Adds an empty level to the project and starts editing it |
| `open_level` | Edits another of the project's levels, replacing the document, the selection and the history with it |

### Conventions worth knowing before calling one

- **Levels.** A level is a whole document. `create_level` and `open_level`
  replace the placements, the lights, the selection and the undo history in
  one go, and both refuse outright while the open level holds edits its file
  does not have. Pass `"unsaved": "discard"` to lose them deliberately, or
  `run_command` with `save` first to keep them.
- **Axes.** Zero yaw, so **+X is right across the screen**, +Y runs away
  from the camera (down-screen), +Z is straight up. One unit is one tile.
  The manifest repeats this under `axes`.
- **Targets.** Every tool that names an entry takes `target` of
  `"placement"`, `"light"`, `"player_start"`, or `"selection"`, and an
  `index` for all but the last. `"selection"` means whatever the properties
  panel is on.
- **Editing selects.** A tool that changes an entry selects it, so the
  viewport outlines what just moved. `delete` is the exception that proves
  it: what it removed cannot be outlined, so the selection is cleared.
- **Removing renumbers.** Deleting an entry moves everything after it in
  that list down one, and the other lists are untouched. Remove several by
  index back to front, or re-read `list_placements` between calls.
- **A write that changes nothing records nothing.** `changed` comes back
  false and the history does not grow — the same rule a property drag that
  ends where it began follows.
- **Values are normalised on the way in.** Angles wrap into [-180, 180),
  colour channels and direction components clamp, multipliers hold at or
  above zero, and a player rounds to a whole one from 1 to 4. The response reports what was actually stored, not what was
  asked for.
- **Fields follow what a kind stores, not what the panel shows.** A light
  stores everything but a rotation and a player; a placement stores a
  position and a rotation; a player start stores a position and a player. A directional light's position is only where its marker sits, so
  the panel hides it — but it is real, and this API will move it.

### The worked example

"Shift the only light source to the right by 10."

```bash
curl -s -X POST 127.0.0.1:8787/tools/list_lights -d '{}'
# → one light, index 0, at x 5
curl -s -X POST 127.0.0.1:8787/tools/translate \
     -d '{"target": "light", "index": 0, "dx": 10}'
# → {"status":"ok","changed":true,"result":{"position":{"x":15.0,...}}}
```

One `translate` is one history entry: Ctrl+Z in the window puts it back.

---

## 6. Adding a tool — the rule

**Every tool the editor gains is added to this API in the same change.** Not
as a follow-up, and not when somebody notices: an editor capability an agent
cannot reach is a capability that, from the outside, does not exist. The
capability table in [capabilities.md](capabilities.md) is where that is
tracked, and a row there is not done until its API column says so.

Most of this is enforced rather than remembered. Adding an `AgentTool`
without a schema row fails a `static_assert`; without a dispatch entry, the
same. A `EditorMenuCommand`, `EditorPropertyField`, or `EditorTool` added
without an agent name fails a `static_assert` on its names table, and
[`test_agent_tool_info.cpp`](../../src/editor/agent/test/test_agent_tool_info.cpp)
fails if any of them is unreachable or ambiguously named.

### The checklist

1. **Add the enumerator** to `AgentTool` in
   [`agent-tool.h`](../../src/editor/agent/include/editor/agent/agent-tool.h),
   and to `AGENT_TOOLS` below it.
2. **Describe it** in `AGENT_TOOL_INFO` in `agent-tool-info.h`: a name, a
   summary written for a reader with no other documentation, an
   `AgentToolEffect`, and a `std::span` of `AgentParam` if it takes any.
   Every parameter needs a description an agent can act on — the accepted
   words, and what an omitted optional one defaults to.
3. **Implement it.** A read goes in
   [`agent-state-json.cpp`](../../src/editor/agent/src/agent-state-json.cpp);
   a write in
   [`agent-commands.cpp`](../../src/editor/agent/src/agent-commands.cpp),
   recording an `EditorAction` so undo covers it. Anything needing the
   running editor returns an `AgentHostRequest` instead of reaching for a
   widget.
4. **Wire it** into `AGENT_TOOL_FNS` in `agent-dispatch.cpp`, in enum order.
   The `static_assert` there is what catches a missed one.
5. **Test it** in `src/editor/agent/test/`, against a bare
   `EditorShellState`.
6. **Nothing else.** The manifest, the HTTP route, and the MCP tool list are
   all generated from step 2. There is no bridge to update, no schema file
   to copy, and no second list to keep in step.

### When the editor gains a *feature* rather than a tool

A new document type, a new panel, a new authoring gesture — the question to
ask is *what would an agent need to see it and change it?* Usually a read
tool and a write tool, plus a row in the capability table. If the feature
adds a value type, it also needs a wire name in
[`agent-names.h`](../../src/editor/agent/include/editor/agent/agent-names.h),
which is where the `static_assert`s that catch a forgotten one live.

---

## 7. Safety, and what this is not

- **Loopback only.** The socket binds `127.0.0.1`. There is no
  authentication, and that is deliberate: the access control is that the
  port does not exist unless someone asked for it, and cannot be reached off
  the machine when it does.
- **It can quit the editor.** `run_command` with `exit` does what File >
  Exit does. It can also close a project or open another one, and either
  drops the document held in memory — everything placed since the last
  `run_command` with `save`. An agent should read `list_commands` and mean
  it, and save before it closes anything.
- **One caller at a time is assumed.** Requests are answered in the order
  they arrive on a single thread; nothing coordinates two agents editing the
  same document, and nothing needs to yet.
- **This is not a remote-control protocol for the GUI.** There is no "click
  at these pixels". Tools name what the editor holds — a placement, a light,
  a command — because that is what survives the interface being redrawn.

---

*The tool surface lives in [`src/editor/agent/`](../../src/editor/agent/),
the transport in [`src/platform/agent/`](../../src/platform/agent/), and the
MCP bridge in [`scripts/simplish-editor-mcp.py`](../../scripts/simplish-editor-mcp.py).*
