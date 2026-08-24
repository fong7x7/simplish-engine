# Simplish — Dockspace Widget

**Parent document:** [gui.md](gui.md) §4.6
**Status:** Draft — pre-Phase 1
**Last Updated:** 2026-04-13

---

## 1. Overview

The `GuiDockspaceWidget` is a generic `engine/gui` primitive that subdivides a single rectangular region among named child panels (top bar, bottom dock, left/right side panels, centre fill, floating overlays). It owns region allocation for a multi-panel layout so that individual panels never compute their own screen rect.

The primitive is consumed by both the editor (menu bar + viewport + asset browser + side panels) and the game runtime (HUD slots: top, bottom, corners, centre reticle). There is a single dockspace implementation; applications differ only in which children they attach and which regions those children occupy.

This replaces today's pattern in which ~40 editor panels each pick their own `rect` in `layout()` (see commit `9f33996d`, `simplish-editor.cpp`, `editor-shell-lifecycle.cpp`). Overlap bugs in the editor are a direct symptom of that pattern.

---

## 2. Goals

- **Eliminate panel-computed screen rects.** Panels receive their available rect from the dockspace; they never compute it from window size.
- **Make overlaps impossible by construction.** Two docked panels cannot share the same region; the widget tree enforces this during layout.
- **One primitive for editor and game.** Both the editor shell and the game HUD compose a dockspace; the primitive lives in `engine/gui/`.
- **Inspectable layout contract.** Dockspace children, their assigned regions, and reserved sizes are readable from the widget tree — test harness, debug overlay, and snapshot tests can all query them.
- **Data-driven layout presets.** Editor workspace presets and HUD layout variants are expressible as JSON that declares which panel goes in which region.
- **Progressive capability.** M1 delivers fixed-edge docking (top/bottom/left/right/centre). Tabbed docks, split regions, float windows, and drag-to-rearrange are deferred to M9.

---

## 3. Behaviours

- **B1** Reserve fixed-size regions along the top, bottom, left, and right edges of the dockspace rect; the remaining interior is the centre region.
- **B2** Assign exactly one child widget to each occupied region; the dockspace sets that child's arranged rect during its own `computeLayout` pass.
- **B3** Support a configurable reserved size per edge (e.g. menu bar = 28 px top, asset browser = 200 px bottom) sourced from a config struct.
- **B4** Accept a list of **floating overlay** children that may render anywhere within the dockspace; these do not consume edge budget and are drawn above docked children.
- **B5** Recompute all child rects when the dockspace's own rect changes (window resize).
- **B6** Expose a read-only query for "what is the current rect of region X?" so tests and debug tools can assert placement.
- **B7** Assert in debug builds that no two docked regions have intersecting rects and that all rects stay inside the dockspace rect.
- **B8** Route input events to whichever docked or overlay child contains the cursor, respecting z-order (overlays above docked panels).
- **B9** Serialise the dockspace configuration (which child in which region, reserved sizes) to and from JSON.

---

## 4. Non-Goals (M1 Scope)

The following are deferred to later milestones and should not be implemented in the first version:

- Tabbed docks (two panels sharing a region, one visible at a time).
- Split regions (a region subdivided into two user-draggable halves).
- Float windows (a panel detached into a separate OS window).
- Drag-and-drop rearrangement of panels at runtime.
- Per-user workspace presets stored outside the project tree.
- Gamepad-driven focus across docked regions (handled by the existing focus traversal once panels receive rects).

M1 is fixed-layout dockspace only. This is sufficient to eliminate the overlap class of bugs without committing to the full M9 docking system.

---

## 5. Integration Points

| Consumer | Use |
|---|---|
| Editor shell (`editor/shell/src/simplish-editor.cpp`) | Root dockspace; docks menu bar, viewport, asset browser, side panels. |
| Game HUD (future, M7) | Root dockspace; docks health bar, minimap, chat log, interaction prompt. |
| `GuiWidgetTree` | Dockspace is a `GuiWidget` subtype; lives in the tree like any other widget. No new tree APIs required. |
| `LayoutStyle` | Docked children use `PositionMode::ABSOLUTE` with rects set by the dockspace. Overlay children use `PositionMode::FLOATING` (new variant, see §7). |
| Test harness (`src/engine/gui/test/support/`) | Must provide `assertNoOverlaps`, `assertInsideRegion`, and region-lookup helpers so any widget test can validate a dockspace-based tree. |
| Theme / config | Reserved edge sizes (menu-bar height, asset-browser height) are config-struct fields loaded from JSON per CLAUDE.md "config structs over constexpr". |

---

## 6. Cross-References

- CLAUDE.md coding invariants: context objects, 16-line functions, config-structs-over-constexpr, one-type-per-file, member comments.
- [gui.md](gui.md) §4.6 — top-level docking feature this primitive implements in its M1 form.
- [gui.md](gui.md) §9.1, §9.3 — `DockArea`, `DockPanel` editor-only widgets; `GuiDockspaceWidget` is the generic core those later evolve to wrap.
- Existing `LauncherWidget` (`editor/shell/src/launcher-widget.cpp`) — one-off manual region allocator; will be migrated to use `GuiDockspaceWidget`.

---

## 7. Open Questions

Per `docs/development/REQUIREMENTS.md §1.1`, these must be resolved before Phase 1 finalises. All resolved 2026-04-13.

- [x] **Q1 — Edge precedence.** *Resolved:* horizontal edges (top/bottom) span full width; vertical edges (left/right) occupy the remaining height between top and bottom. Matches current editor behaviour so migration is a no-op for existing panels. Pinned in ADR-013 in the originating project (not carried over) §Consequences; encoded in `computeDockRegions` (Phase 1 leaf §2.2).
- [x] **Q2 — Floating overlay semantics.** *Resolved:* overlays (tooltips, command palette, notification toasts) remain as tree-level components registered via `GuiWidgetTree::registerComponent`; the dockspace owns docked panels only. Keeps M1 scope bounded and preserves the existing overlay contract.
- [x] **Q3 — Config source.** *Resolved:* per-dockspace `GuiDockLayout` config struct, loaded from `data/config/editor/dock-layout.json` (editor) and `data/config/game/hud-layout.json` (game HUD). Passed at construction. Independently tunable per application.
- [x] **Q4 — Minimum window size.** *Resolved:* centre region clamped to zero rect when the viewport is smaller than `sum(edge reserved sizes)` on an axis. `ENGINE_ASSERT` in debug; `Log::warn` in release. Dockspace never produces negative-size rects. Edge cases E1 and I-clamping logic in the Phase 1 leaf cover this.
- [x] **Q5 — Empty region.** *Resolved:* an empty region (no child assigned) contributes zero budget; the reserved size applies only when a child is actually docked. The freed space is claimed by the next region on the same axis.
- [x] **Q6 — Hot-reload of dock config.** *Resolved — deferred to a follow-up PR.* The M1 scope ships `DockspaceConfigLoader::load` (one-shot) and `GuiDockspaceWidget::setLayout` (replace-layout) but **does not register a filesystem watcher**. Callers wanting hot-reload wire the existing engine filesystem-watcher utility to `load` + `setLayout` themselves; a first-class watcher API is out of M1 scope and tracked separately (see the Phase 4 review finding S2).
- [x] **Q7 — Migration plan for existing panels.** *Resolved:* first migration covers menu bar + viewport + asset browser (the panels with prior overlap fixes). Remaining ~37 panels migrate panel-by-panel in follow-up PRs and do not block M1 completion. Transition period tolerates both patterns in the tree.

Phase 1 finalisation is unblocked.

---

## 8. Migration Lessons Learned (2026-04-14 attempt)

A first attempt at editor migration was made and reverted after surfacing blocking integration issues. Captured here for the next attempt.

### What failed

The editor shell's chrome panels (menu bar, right panel, asset browser, status bar) each provide their own `widget->layout(tree, rect)` method that internally positions their child widgets (e.g. menu bar lays out dropdown buttons horizontally). In the pre-migration world, `tree.computeLayout` was never called per frame — `tickEditor` manually invoked each panel's `.layout(tree, rect)` with a hand-computed outer rect.

The migration tried to move to a tree-driven layout: insert `GuiDockspaceWidget` as root child, assign chrome panels to dock edges, call `tree.computeLayout(viewport)` once per frame. Two problems surfaced:

1. **Default `arrangeChildren` is not recursive.** Setting `child->rect = slice` does not trigger the child's own `arrangeChildren`. So when `tree.computeLayout` arranges root, it sets the dockspace's rect but never runs the dockspace's own arrange → dock children never get their rects.

2. **Making the default recursive breaks chrome panels.** If the default `arrangeChildren` calls `tree.arrangeWidget(child_id, slice)` (thereby triggering each child's own arrange), the chrome panels' children (menu bar's dropdown buttons, right panel's contents, etc.) get column-sliced by the default column-arrange — which has nothing to do with their actual `.layout(tree, rect)` semantics. Menu dropdowns collapse to stacked rects, right-panel contents misalign.

### What the next attempt needs

Either of these architectural moves (not both):

- **(a) Override `arrangeChildren` on every chrome panel class** to delegate to its existing `.layout(tree, available)` method. This makes tree-driven recursive arrange work correctly for each panel. Requires touching `MenuBarWidget`, `StatusBarWidget`, `RightSidePanelWidget`, and anything with a custom `.layout(tree, rect)` signature. Lowest-risk path if done panel-by-panel with unit tests per panel.

- **(b) Keep default `arrangeChildren` non-recursive, and invoke panel layout in two passes per frame.** Pass 1: explicitly `tree.arrangeWidget(dockspace_id, viewport)` to set outer rects on dock children. Pass 2: after that pass, iterate the chrome panels and call each `->layout(tree, ->rect)` to fix up their internal layout. Requires no widget-class changes but keeps the two-phase discipline forever — and is still prone to forgetting one panel.

Option (a) is the clean long-term shape. Option (b) works as a transitional mechanism.

### Ordering gotcha

`tickEditorShellState` calls `state.asset_browser->layout(tree, state)` — the asset browser's internal layout uses `this->rect` implicitly, so the outer rect must be set BEFORE this call. If the migration moves to `tree.computeLayout`, it must run **before** `tickEditorShellState`, not after.

### Visual-verification checkpoint

The failure mode the user observed confirms that panels' *outer* rects are being set by the dockspace but their *inner* widgets are being broken by the recursive default arrange. Any future migration attempt should build the editor, launch it interactively, and verify:
- Menu bar dropdowns (File / Edit / View / …) render as horizontal clickable buttons — not stacked.
- Right panel contents are not collapsed to a single vertical stack.
- Asset browser icons/folders render in their normal grid, not a column slice.
- Viewport occupies the full centre region (no top-left clumping).

If any of these visibly fails, the migration has triggered the "recursive default arranges panel internals" bug.

---

## 9. Agent reliability traps (2026-04-14 follow-up — what burned the second migration)

> **Single-page action reference: `docs/development/agent-ui-quickstart.md`.** Read it first. This section is the deep-dive prose for the four traps the quickstart summarises in a table.

The second migration attempt landed (commits `4c28bbbe` … `fd8ffd02`) but only after surfacing four traps that are not obvious from reading the production code. Each is now testable; agents working on chrome layout should read this section before touching widget hierarchy or input dispatch.

### 9.1 `insertExternalWidget(parent = INVALID)` silently re-roots the tree

Every call with `parent = GUI_WIDGET_ID_INVALID` overwrites `tree.root_id` with the new widget. Calling it twice means the second insert silently replaces the root. The pre-migration `insertWidget<T>` helper in `editor-shell-lifecycle.cpp` exploited this — the first insert won, every subsequent `tree.root_id` then pointed at that first widget, so `insertWidget(... tree.root_id)` re-parented later widgets as cascading descendants. Any new architectural insertion must:

- Insert a single explicit root container (`ensureTreeRoot` pattern in `editor-shell-lifecycle.cpp`).
- Pass `tree.root_id` (the explicit container id) as the parent for siblings.
- Never call `insertExternalWidget(... INVALID)` more than once per tree.

### 9.2 Hit-test aborts on the first ancestor whose rect is zero

`gui-input.cpp:hitTestRecursive` checks `rectContains(parent.rect, x, y)` and returns immediately if the parent does not contain the point — the descendants are never visited even if their own rects would match. A container with default `rect = {0, 0, 0, 0}` makes everything inside it unclickable. The visual test PNG looks correct, render works, but every click silently fails.

Production fix in `simplish-editor.cpp::tickDockspace`: the explicit shell-root panel is sized to the full viewport before each arrange. Test lock: `clickReachesWidget(tree, x, y, expected_id)` in `src/engine/gui/test/support/gui_layout_validator.h`. Use it from any visual test that exercises chrome layout.

### 9.3 Pure layout containers must override `render()` to a no-op

`GuiPanel::render` paints a filled rectangle. A widget that exists only to arrange its children (`GuiDockspaceWidget`, `EditorCenterContentWidget`) inheriting that default will paint an opaque box over everything beneath it. Both production layout containers override `render(const GuiDrawContext&) const` to do nothing — a new composite must do the same. Symptom: a region renders as solid black or solid theme colour with the children invisible underneath.

### 9.4 Labels with non-empty text and zero rect leak to (0, 0)

`GuiLabel::render` and `GuiButton::render` paint at `rect.x, rect.y` with no clipping. A widget whose `init` creates labels but whose `layout` only updates `text` (the long-standing pattern in `PhysicsDebuggerPanel`, `CommandPaletteWidget`, `NotificationWidget`, `SimulationOverlayWidget`, and `AiPromptWidget`) renders every label at the screen origin — overlapping the menu bar with garbled stacked text. Editor-shell `tickShellWidgets` sets text every frame regardless of whether positioning ran.

The shipped pattern now is **layout every overlay every frame, regardless of visibility**, paired with internal visibility management on the widget's render-bearing root child. `SimplishEditor::layoutOverlays` calls `layout(tree, viewport)` on `command_palette`, `simulation_overlay`, and `notification_widget` after `tickEditorShellState` has run (so newly-added notification entries get their rects on the same frame they appear). Each widget's `tick` then sets its internal root child's `visible` flag based on state — the outer widget stays visible-by-default but its render-bearing subtree only paints when state says it should.

For widgets that don't yet manage internal visibility (`PhysicsDebuggerPanel`, `AiPromptWidget` in some configurations), the workaround is `visible = false` on the outer widget after init in `editor-shell-lifecycle.cpp` — but the *proper* fix for those is to (a) wire a real `layout(tree, rect)` call from `layoutOverlays`, (b) flip the internal root child's visibility from tick based on the relevant state predicate, and (c) revert the outer-widget hack. AiPromptWidget already has `root_panel_->visible = state.is_open` in its tick, so it only needs a layout site. PhysicsDebuggerPanel needs both.

Test lock: `findUnpositionedVisibleText(tree)` in `src/engine/gui/test/support/gui_layout_validator.h`. Use it from any visual test that initialises real chrome widgets — invoke after layout + tick have run for the frame.

### 9.5 Test-harness recipe

The combined recipe used by `tests/editor/shell/test_editor_shell_visual.cpp` — copy this for any future chrome-related visual test:

```cpp
// 1. Init the production chrome.
REQUIRE(initEditorShellState(state, gui, config, menus));

// 2. Mimic tickEditor: size shell-root, arrange dockspace, run one tick,
//    then layout the viewport-sized overlays (production does this in
//    SimplishEditor::layoutOverlays AFTER tickEditorShellState so
//    newly-arrived notification entries get rects this frame).
auto* shell_root = gui.tree->findWidget(gui.tree->root_id);
REQUIRE(shell_root != nullptr);
shell_root->rect = viewport;
gui.tree->arrangeWidget(state.dockspace_id, viewport);
REQUIRE(tickEditorShellState(state, 1.0F / 60.0F));
state.command_palette->layout(*gui.tree, viewport);
state.simulation_overlay->layout(*gui.tree, viewport);
state.notification_widget->layout(*gui.tree, viewport);

// 3. Render to a vertex buffer + rasterise to PNG for human review.
gui.tree->renderAll(ctx);

// 4. Lock in the regression invariants.
REQUIRE(test::findUnpositionedVisibleText(*gui.tree).empty());
REQUIRE(test::clickReachesWidget(*gui.tree, menu_x, menu_y,
                                 state.menu_bar->widget_id));
REQUIRE(test::clickReachesWidget(*gui.tree, right_x, right_y,
                                 state.right_panel->widget_id));
```

The two `test::` helpers ship in `src/engine/gui/test/support/gui_layout_validator.h`; the editor-shell tests link them via the `target_include_directories` line in `tests/editor/CMakeLists.txt`.

### 9.6 What an agent should run before declaring chrome work done

1. `[editor-shell-visual]` and `[layout-validator]` test tags green.
2. The PNG at `build/debug/editor-shell.png` shows the expected layout (no black regions, no overlap, panels in their assigned dock edges).
3. Launch the actual `./build/debug/editor/simplish-editor` binary and click each menu item — render passes do not exercise input dispatch, so this is not redundant.

---

*This is a living requirements doc. Update as design decisions land in ADRs or the Phase 1 leaf resolves open questions.*
