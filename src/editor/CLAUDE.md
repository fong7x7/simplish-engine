# editor

The desktop authoring application. Two packages: `project/` (project format,
open and create, recent list) and `shell/` (title bar, menus, toolbar,
viewport, asset browser, properties panel). Links `platform` and `engine`;
namespace `eng::editor` throughout.

## Read first

- [docs/editor/REQUIREMENTS.md](../../docs/editor/REQUIREMENTS.md) — what the
  editor must do, and its §1 *Current State* for what is actually built.
- [docs/editor/project-format.md](../../docs/editor/project-format.md) — the
  on-disk format. Only `.simplish/project.json` exists today; levels,
  encounters, scenarios, and data tables are specified but unwritten.

## Fixed decisions — do not re-derive these

- **The projection is 4:3 dimetric with zero yaw**, not 45° isometric: tiles
  are axis-aligned, 64 px wide, foreshortened to 48 px deep, with an
  unforeshortened height axis (`ISO_TILE_WIDTH` / `_DEPTH` / `_RISE` in
  [iso-projection.h](shell/include/editor/shell/iso-projection.h)). Sprites are
  authored against these constants — they are constants, not settings. Closed
  as Open Question 1 on 2026-08-26; see the
  [ADR-003 amendment](../../docs/decisions/ADR-003-hybrid-iso-render-model.md#amendment-2026-08-26-straight-on-projection).
- **The camera never rotates.** Code may depend on that (design principle 4).
- **Undo is one list plus a cursor**, not two stacks — `[0, applied)` are in
  effect, `[applied, size)` are undone and redoable, and a new edit after an
  undo truncates. See [editor-action-history.h](shell/include/editor/shell/editor-action-history.h).
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

`SIMPLISH_LLDB=1` runs it under LLDB and prints a backtrace on a crash;
`SIMPLISH_LLDB=i` drops into interactive LLDB. macOS also writes crash reports
to `~/Library/Logs/DiagnosticReports/simplish-editor-*.ips`.
