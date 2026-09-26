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
- **HarfBuzz shaping, ICU line breaking and the SDF atlas are described but
  absent.** FreeType is built with `FT_DISABLE_HARFBUZZ`. What ships —
  sized and weighted faces, `kern`-table kerning, UTF-8, word wrap and
  ellipsis — is in `technical/text-pipeline.md` §0; the sections after it
  are the original design.
- **The dev console does not exist.** Gamepad navigation does, and is newer
  than the spec around it: `technical/input.md §5` describes what shipped.

## How GUI code is actually tested here

A plain Catch2 fixture that owns a `GuiWidgetTree` and inserts the widget under
test — no window, no GPU, no harness. `src/editor/shell/test/test_editor_toolbar_widget.cpp`
is the pattern to copy.

For anything visual, `gui-software-rasterizer.cpp` renders the tree to a pixel
buffer in-process, so appearance is assertable without a device. The editor's
`test_editor_*_capture.cpp` tests use it and write `*-capture.png` at the repo
root — gitignored artifacts, not fixtures to commit.

## Notes

- Layout is CSS flexbox on `tree_layout` (`layout-engine.h`); run by
  `GuiWidgetTree::computeLayout`. Style widgets and let it place them —
  do not compute rects by hand in new code. Recipes, the style reference
  and the differences from CSS are in
  [technical/layout-engine.md](../../../docs/engine/gui/technical/layout-engine.md).
  A widget with a natural size overrides `measureContent`; one with its
  own placement rule overrides `arrangeChildren` and calls
  `tree.arrangeWidget` on each child; one placed by another widget is
  `PositionMode::MANUAL`.
- Anything that floats — a menu, popover, dialog, toast — goes in
  `tree.overlayLayer()` and is placed with `placePopover` or insets
  ([technical/overlays.md](../../../docs/engine/gui/technical/overlays.md)).
  There is no other overlay mechanism; `registerComponent` is gone.
- Before writing a widget, check the catalogue in `technical/widgets.md` §0:
  checkbox, toggle, radio, tabs, progress, number field, virtual list, table
  and tree view exist.
- Widgets subclassing the external-widget path are inserted with
  `tree.insertExternalWidget(std::make_unique<T>(), parent)`, then `init(tree)`.
- Colours, spacing and radii come from `ctx.activeTheme()` (`GuiTheme`,
  [technical/theming.md](../../../docs/engine/gui/technical/theming.md));
  do not hardcode colours in a widget. Interaction looks go through the
  `disabled` / `selected` flags and per-state `GuiStateStyles`, which blend
  on change — never by swapping styles each tick.
- A subtree drawn in another theme (a game's screen inside the editor)
  gets `widget.subtree_theme`; the tree scopes measuring, updating and
  drawing to it. Draw through `tree.renderAll(ctx)` — walking
  `visitDrawOrder` yourself skips the scoping.
- `image-loader.cpp` calls stb, whose implementation TUs live in
  `engine/image/` — see the root CLAUDE.md for why.
