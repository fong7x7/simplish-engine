# engine/gui

Retained-mode UI toolkit: widget tree, flexbox layout, FreeType text, RHI
quad-batch renderer, theming, docking, markdown. Platform-agnostic — no SDL,
no graphics API, no editor or game types below this line.

## Read the technical doc for the subsystem you are touching

[docs/engine/gui/README.md](../../../docs/engine/gui/README.md) has one
document per subsystem, each naming the exact source files it describes —
widget tree, layout engine, renderer, text pipeline, theming, input, dockspace,
markdown parser and renderer, tables, testing. Read the one that matches your
file before changing behaviour; these documents describe the shipped code.

## Provenance — where the docs and the code disagree

This framework was copied in from a 3D voxel engine. It is domain-neutral and
applies unchanged, but the docs carry claims that are **not true of this
repository**. Verify before relying on any of them:

- **`docs/engine/gui/technical/testing.md` describes a test harness that does
  not exist here** — `GuiTestHarness`, `GuiLocator`, `GuiExpect`,
  `GuiRenderExpect`, and `test/support/` were not carried over. Do not write
  tests against them.
- **`gui.md §9` is an editor contract from that other project**, not the
  Simplish editor. Treat it as a template; [docs/editor/REQUIREMENTS.md](../../../docs/editor/REQUIREMENTS.md)
  is authoritative.
- **HarfBuzz shaping and ICU line breaking are described but absent.** FreeType
  is built with `FT_DISABLE_HARFBUZZ`; `shaped-run.cpp` handles kerning and
  ligatures with no complex-script support.
- **The dev console and gamepad navigation do not exist.** Gamepad nav depends
  on `engine/input`, which is not written.

## How GUI code is actually tested here

A plain Catch2 fixture that owns a `GuiWidgetTree` and inserts the widget under
test — no window, no GPU, no harness. `src/editor/shell/test/test_editor_toolbar_widget.cpp`
is the pattern to copy.

For anything visual, `gui-software-rasterizer.cpp` renders the tree to a pixel
buffer in-process, so appearance is assertable without a device. The editor's
`test_editor_*_capture.cpp` tests use it and write `*-capture.png` at the repo
root — gitignored artifacts, not fixtures to commit.

## Notes

- Widgets subclassing the external-widget path are inserted with
  `tree.insertExternalWidget(std::make_unique<T>(), parent)`, then `init(tree)`.
- Theme tokens go through the scope stack in `gui-theme.cpp`; do not hardcode
  colors in a widget.
- `image-loader.cpp` calls stb, whose implementation TUs live in
  `engine/image/` — see the root CLAUDE.md for why.
