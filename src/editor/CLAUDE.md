# editor

The desktop authoring application. Three packages: `project/` (project
format, open and create, recent list), `shell/` (title bar, menus, toolbar,
viewport, asset browser, properties panel), and `agent/` (the tool surface
agents drive the editor through). Links `platform` and `engine`; namespace
`eng::editor` throughout.

## Read first

- [docs/editor/REQUIREMENTS.md](../../docs/editor/REQUIREMENTS.md) — what the
  editor must do, and its §1 *Current State* for what is actually built.
- [docs/editor/project-format.md](../../docs/editor/project-format.md) — the
  on-disk format. Only `.simplish/project.json` exists today; levels,
  encounters, scenarios, and data tables are specified but unwritten.
- [docs/editor/agent-api.md](../../docs/editor/agent-api.md) — the agent API,
  and §6's checklist. **Read it before adding a tool, a panel, or a menu
  command**, because exposing it is part of the same change.
- [docs/editor/capabilities.md](../../docs/editor/capabilities.md) — what the
  editor can do today and whether an agent can do it too. Update the row in
  the change that moves it.

## Fixed decisions — do not re-derive these

- **The projection is a project's setting, and there are exactly two of
  them** — 4:3 dimetric with zero yaw (the default, 64×48 axis-aligned tiles)
  and 2:1 isometric (64×32 diamonds). Both are held as an `IsoAxes` in
  [iso-axes.h](shell/include/editor/shell/iso-axes.h); which one a project
  uses is a `ProjectProjection` in its `project.json`. Nothing may reintroduce
  a compile-time constant for the axes: derive from the axes instead, the way
  `isoProjectionRay` derives the direction points collapse along. `ISO_TILE_WIDTH`
  is the one number both share. See the
  [ADR-003 amendment](../../docs/decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-09-08-projection-as-a-project-setting).
- **Both projections are rotations of the world, never shears.** The height
  scale is derived by `isoRiseFor`, not chosen: the two rows of the
  projection have to be perpendicular and the same length, or round things
  draw as ovals. This is invisible in anything built from the axes — tiles,
  grid, cubes and bounds all agree either way — so it is asserted directly
  in `test_iso_projection.cpp` and end-to-end by rendering a sphere in
  `test_editor_mesh_capture.cpp`. Height is therefore foreshortened by the
  camera's pitch; do not "fix" that back.
- **Sprites and tile art are authored against one projection.** Switching a
  project's projection rotates the world under its art, which is why the
  switch writes itself into the project rather than into an editor
  preference.
- **The camera never rotates freely.** Two fixed yaws are not a rotating
  camera; code may depend on the projection being constant for the frame
  (design principle 4).
- **Undo is one list plus a cursor**, not two stacks — `[0, applied)` are in
  effect, `[applied, size)` are undone and redoable, and a new edit after an
  undo truncates. See [editor-action-history.h](shell/include/editor/shell/editor-action-history.h).
- **The agent API is a pure function of `EditorShellState`.** `agent/` may
  read and edit that state, and hands back an `AgentHostRequest` for the
  three things only the running editor can do. The shell knows nothing about
  it: `SimplishEditor::setStateHook` is a generic per-tick callback, and
  `agent/` depends on `shell/`, never the other way round.
- **A tool is described in exactly one place** — `AGENT_TOOL_INFO`. The HTTP
  manifest is generated from it and the MCP bridge reads that manifest at run
  time, so never write a second list of tools anywhere.
- **The recent-projects list is written outside the checkout** when the
  platform offers a user data directory. `data/editor/recent-projects.json` is
  gitignored on purpose — it belongs to whoever runs the editor.

## Conventions in this subtree

- Headers open with `/// @file`, `/// @brief`, and `/// @par Threading`. This
  is the newer style; match it here rather than the `DESIGN SUMMARY` block used
  by the copied platform headers.
- Widget tests are plain Catch2 fixtures owning a `GuiWidgetTree`, no window
  and no GPU — [test_editor_toolbar_widget.cpp](shell/test/test_editor_toolbar_widget.cpp)
  is the pattern.
- `test_editor_*_capture.cpp` render through the GUI software rasterizer and
  write `*-capture.png` at the repo root. Those PNGs are gitignored artifacts
  for eyeballing output; they are not golden images and nothing compares
  against them.
- Failure is in the signature: `project-open-result.h` / `project-open-error.h`
  carry the error, and there are no exceptions anywhere.

## Running it

```bash
./scripts/editor.sh [PROJECT_DIR]
```

`SIMPLISH_AGENT_PORT=default` opens the agent API on 127.0.0.1:8787 — see
[agent-api.md](../../docs/editor/agent-api.md).

`SIMPLISH_LLDB=1` runs it under LLDB and prints a backtrace on a crash;
`SIMPLISH_LLDB=i` drops into interactive LLDB. macOS also writes crash reports
to `~/Library/Logs/DiagnosticReports/simplish-editor-*.ips`.
