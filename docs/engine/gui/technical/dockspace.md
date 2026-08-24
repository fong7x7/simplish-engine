# Technical Approach — Dockspace Widget

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [dockspace.md](../dockspace.md)
**Library:** engine/gui
**Date:** 2026-04-13
**Status:** Draft — Review Cycle 0

---

## 1. Requirements Analysis

### 1.1 Behaviours

- B1: Reserve fixed-size edge regions (top, bottom, left, right) inside a parent rect; interior remainder is the centre region
- B2: Arrange exactly one docked child per occupied region; dockspace sets each child's rect during its own arrange pass
- B3: Source reserved edge sizes from a `GuiDockLayout` config struct passed at construction
- B4: Recompute all child rects when the dockspace's own rect changes (window resize)
- B5: Expose a read-only `regionRect(DockEdge)` query for tests and debug overlays
- B6: Assert in debug builds that docked child rects do not intersect and stay inside the dockspace rect
- B7: Serialise dock assignments (which child id in which region, reserved sizes) to/from JSON
- B8: Allow a region to be empty (no child assigned) — empty regions contribute zero budget
- B9: Participate in the standard widget tree lifecycle (clone, update, render, input dispatch)

### 1.2 Edge Cases

- E1: Window smaller than `sum(edge reserved sizes)` on an axis — clamp centre to zero rect; assert in debug; log-and-continue in release
- E2: Region with no assigned child — zero budget, next region in that edge group claims the freed space
- E3: Child assigned to a region that is later removed — child remains a widget tree node but is not arranged (zero rect, not hit-tested)
- E4: Two children assigned to the same region at construction — reject second assignment; log error with both child ids
- E5: Dockspace rect has zero width or height — all children get zero rects; no asserts fire (legitimate during startup before first resize)
- E6: Config specifies negative or NaN reserved size — reject at load; fall back to default per edge (`MENU_BAR_DEFAULT_HEIGHT` etc.)
- E7: Dockspace nested inside another dockspace — supported; inner dockspace receives its parent's region rect as its own viewport
- E8: Edge precedence at corners — horizontal edges (top/bottom) span full width; vertical edges (left/right) occupy remaining height between top and bottom (see ADR-013, §Edge Precedence)

### 1.3 Invariants

- I1: Docked children always have `PositionMode::ABSOLUTE` and their rect is set by the dockspace, never by themselves
- I2: No two docked children's rects intersect (enforced in debug by post-arrange assertion)
- I3: The union of docked child rects plus the centre rect equals the dockspace rect exactly (no gaps, no overflow)
- I4: A child's arranged rect stays inside the dockspace rect (`child.rect ⊆ dockspace.rect`)
- I5: `GuiDockLayout` sizes are immutable after construction; to change reserved sizes, replace the dockspace's layout and mark dirty
- I6: Region-to-child assignment is stable within a frame; swapping assignments mid-frame is undefined

### 1.4 Integration Points

- `GuiWidget`: `GuiDockspaceWidget` is a `GuiWidget` subtype; overrides `clone`, `render`, and participates in the tree's existing measure/arrange pass
- `GuiWidgetTree::arrangeWidget`: the tree calls the dockspace's arrange; the dockspace then sets child rects directly via the widget tree's `arrangeWidget(child, region_rect)`
- `LayoutStyle::PositionMode::ABSOLUTE`: docked children must use this position mode; dockspace writes `abs_x`, `abs_y`, `width`, `height`
- `engine/core/config`: `GuiDockLayout` is loaded via the existing JSON config loader; hot-reload callback wires to dockspace instance
- Editor shell (`editor/shell/src/simplish-editor.cpp`): replaces the manual rect computation for menu bar, viewport, and asset browser (migration in a separate PR)
- `src/engine/gui/test/support/`: new helpers `assertNoOverlaps(std::span<Rect>)` and `assertInsideRect(Rect child, Rect parent)` are consumed by dockspace tests and any widget test that composes panels

### 1.5 Extension Points

- `GuiDockLayout` accepts additional named regions beyond the four edges + centre via a `std::vector<DockRegion> extra_regions` field (reserved for M9 split/tab support). For M1 this vector is always empty.
- Applications compose their own `GuiDockLayout` — the primitive makes no assumption about which regions are occupied. Editor and game HUD each load their own layout JSON.

---

## 2. Design

### 2.1 Context Objects

```cpp
/// Which edge of the dockspace a child is docked against. CENTRE is the
/// interior rect remaining after all edges are reserved.
enum class DockEdge : uint8_t { TOP, BOTTOM, LEFT, RIGHT, CENTRE };

/// One region in a dockspace layout: which edge it occupies and how many
/// pixels it reserves. CENTRE regions ignore size_px (they consume the
/// remainder).
struct DockRegion {
  /// Edge this region is docked against.
  DockEdge edge;
  /// Reserved size in pixels. Height for TOP/BOTTOM; width for LEFT/RIGHT;
  /// ignored for CENTRE.
  float size_px;
  /// Widget id of the child assigned to this region (GUI_WIDGET_ID_INVALID
  /// if the region is empty).
  GuiWidgetId child;
};

/// Layout configuration for a single dockspace instance. Loaded from
/// `data/config/editor/dock-layout.json` (editor) or
/// `data/config/game/hud-layout.json` (game HUD). Immutable after
/// construction; to change, replace the dockspace's layout and mark dirty.
struct GuiDockLayout {
  /// Fixed region slots (one per edge plus centre). Empty regions have
  /// child == GUI_WIDGET_ID_INVALID.
  std::array<DockRegion, 5> regions;
  /// Reserved for M9 split/tab extensions. Always empty in M1.
  std::vector<DockRegion> extra_regions;
};
```

### 2.2 Public Interface

File: `src/engine/gui/include/engine/gui/gui-dockspace-widget.h`

```cpp
/// Widget that subdivides its own rect into named regions and assigns each
/// region's rect to a docked child. See docs/engine/gui/dockspace.md.
/// @thread_safety Main thread only.
class GuiDockspaceWidget : public GuiWidget {
public:
  /// Construct with a layout config. The layout's child ids must already
  /// exist in the widget tree that will adopt this dockspace.
  explicit GuiDockspaceWidget(GuiDockLayout layout);

  /// Assign a child widget to a region. Replaces any previous assignment.
  /// Returns false if the region is out of range or the child is already
  /// assigned elsewhere in this dockspace.
  bool assignChild(DockEdge edge, GuiWidgetId child);

  /// Clear the child assignment for a region. The child widget is not
  /// destroyed; it simply stops being arranged.
  void clearRegion(DockEdge edge);

  /// Read-only query: the arranged rect of a region. Returns std::nullopt
  /// if the region is empty or has not yet been laid out.
  std::optional<Rect> regionRect(DockEdge edge) const;

  // GuiWidget overrides
  std::unique_ptr<GuiWidget> clone() const override;
  void render(const GuiDrawContext& ctx) const override;

private:
  /// Immutable layout config.
  GuiDockLayout layout_;
  /// Cached arranged rect per region, populated during arrange(). Indexed
  /// by DockEdge.
  std::array<Rect, 5> region_rects_{};
  /// True once arrange() has run at least once with a non-empty viewport.
  bool has_layout_ = false;
};
```

File: `src/engine/gui/include/engine/gui/dockspace-arrange.h`

```cpp
/// Compute region rects for a dockspace given its viewport. Pure function;
/// used both by GuiDockspaceWidget::arrange and by tests. Writes
/// output[edge] for every DockEdge value. Returns true iff the centre
/// region has positive area on both axes; false when centre is clamped
/// to zero (viewport smaller than reserved edges, zero-sized viewport,
/// or edges exactly consuming the viewport).
bool computeDockRegions(const GuiDockLayout& layout,
                        const Rect& viewport,
                        std::array<Rect, 5>& output);
```

File: `src/engine/gui/include/engine/gui/dockspace-config-loader.h`

```cpp
/// Load a GuiDockLayout from a JSON file. Returns nullopt on parse error
/// or invalid values (negative sizes, unknown edge names).
std::optional<GuiDockLayout> loadDockLayout(std::string_view path);
```

### 2.3 Module Decomposition

| File | Responsibility | ~Lines |
|------|---------------|--------|
| `dock-edge.h` | `DockEdge` enum (one type per file per CLAUDE.md) | ~25 |
| `dock-region.h` | `DockRegion` struct | ~30 |
| `gui-dock-layout.h` | `GuiDockLayout` struct | ~30 |
| `dockspace-widget.h` | `GuiDockspaceWidget` class declaration | ~80 |
| `dockspace-widget.cpp` | Construction, assignment, clone, render, arrange glue | ~180 |
| `dockspace-arrange.h`/`.cpp` | Pure `computeDockRegions` function, corner-precedence logic | ~120 |
| `dockspace-config-loader.h`/`.cpp` | JSON parse + validation, hot-reload registration | ~140 |

Total: ~605 lines across 7 translation-unit pairs. Every function ≤ 16 lines; named algorithms in `dockspace-arrange.cpp` up to 50 lines with comment headers (CLAUDE.md invariant).

### 2.4 Data Flow

```
Startup:
  1. loadDockLayout("data/config/editor/dock-layout.json") -> GuiDockLayout
  2. Widget tree creates child panels (menu bar, viewport, asset browser)
  3. GuiDockLayout.regions[*].child populated with tree-assigned widget ids
  4. GuiDockspaceWidget constructed with layout; inserted at tree root

Per frame (inside GuiWidgetTree::computeLayout):
  1. Tree measure pass — dockspace reports its own rect as its parent's full viewport
  2. Tree arrange pass — dockspace.arrange(viewport) invoked by tree
  3. dockspace.arrange() calls computeDockRegions(layout_, viewport, region_rects_)
  4. For each non-empty region: tree.arrangeWidget(region.child, region_rects_[edge])
  5. In debug: assert no two region rects intersect; assert each ⊆ viewport

Hot-reload (deferred — see Q6 and Phase 4 finding S2):
  M1 ships the building blocks (`DockspaceConfigLoader::load`,
  `GuiDockspaceWidget::setLayout`) but does not register a filesystem
  watcher. Callers wire the engine filesystem-watcher utility themselves
  to `load` + `setLayout` if they want dev-build hot-reload. A first-class
  watcher API is out of M1 scope.
```

### 2.5 Error Strategy

| Situation | Response |
|-----------|----------|
| JSON parse failure on initial load | `loadDockLayout` returns `nullopt`; caller falls back to compiled-in default layout |
| Hot-reload parse failure | Log error with path; keep previous `GuiDockLayout` |
| Negative or NaN reserved size in config | Reject at load; `loadDockLayout` returns `nullopt` |
| Viewport too small for reserved sizes (E1) | Clamp centre to zero rect; `ENGINE_ASSERT` in debug; `Log::warn` in release |
| Duplicate child assignment (E4) | `assignChild` returns `false`; `Log::error` with both child ids; caller's state unchanged |
| Unknown `DockEdge` value in config | Reject at load (JSON loader validates against enum names) |
| `regionRect` called before first arrange | Returns `nullopt` |
| Overlap detected in debug assertion (I2 violation) | `ENGINE_ASSERT` fires with offending edge pair and their rects; only possible via programmer error in `computeDockRegions` |

No C++ exceptions. All fallible paths return `std::optional` or `bool` per CLAUDE.md.

### 2.6 Dependency Inventory

| Dependency | Injected via |
|------------|-------------|
| `GuiWidgetTree` | Passed to `arrangeWidget` internally during arrange — dockspace holds no tree pointer; the tree invokes arrange on it |
| `GuiDockLayout` | Constructor parameter; loaded from JSON by caller |
| Filesystem watcher (dev builds) | Optional callback registered by caller; not owned by dockspace |
| JSON loader (`engine/core/config`) | Used only by `dockspace-config-loader.cpp`; not by the widget itself |

### 2.7 Common Module Identification

- `assertNoOverlaps(std::span<Rect>)` and `assertInsideRect(Rect, Rect)` belong in `src/engine/gui/test/support/gui_layout_validator.h` — reusable by every widget test, not dockspace-specific. Added in the same PR as the dockspace tests.
- `computeDockRegions` is kept as a free function (not a method) because it is a pure transformation on external inputs, per CLAUDE.md "Instance methods vs static". Testable without constructing a widget.

### 2.8 Extensibility Design

- `GuiDockLayout::extra_regions` reserved for future split/tab regions (M9); M1 asserts this is empty during load.
- Applications attach any `GuiWidget` subtype as a docked child — dockspace does not care about the child's concrete type. `DockPanel` from `gui.md` §9.3 is one valid child type but not required.
- `GuiDockspaceWidget` can be nested: a centre region's child can itself be a dockspace. This enables editor workspace modes that subdivide the centre further (e.g. Map workspace splits centre into viewport + minimap) without new primitives.

---

## 3. Review Log

### Iteration 0

Initial draft produced from requirements doc (`docs/engine/gui/dockspace.md`). Checklist review not yet run. Open questions Q1–Q7 in the requirements doc must be marked resolved (with ADR citations where applicable) before Iteration 1 review.

### Iteration 1 — 2026-04-13

**Checklist review covered:**

1. Requirements Analysis completeness (behaviours, edge cases, invariants, integration, extension)
2. Context objects documented (every field has `///` comment per CLAUDE.md)
3. Public interfaces respect 4-parameter limit and 16-line function guideline
4. Error strategy covers every failure mode in edge cases
5. No C++ exceptions, no bare `new`/`delete`, no globals/singletons
6. One type per file, UTF-8 strings, `std::optional`/`std::expected` for fallible ops
7. Threading model stated (main thread only) on every public type
8. Config-structs-over-constexpr rule respected (reserved sizes are config, not `constexpr`)
9. Open questions Q1–Q7 resolved in the requirements doc with ADR-013 references
10. Module decomposition adheres to one-type-per-file and has realistic line budgets
11. Naming conventions match CLAUDE.md (`PascalCase` types, `camelCase` methods, `snake_case_` private members, `UPPER_CASE` enum values)

**Gaps identified and closed:**

- Gap 1: Initial `GuiDockspaceWidget::arrange` was under-specified — the leaf didn't explicitly say the tree's `arrangeWidget` is the mechanism by which child rects are set. Clarified in §2.4 Data Flow step 4.
- Gap 2: Edge-precedence rule (Q1) was stated only as a recommendation; pinned in ADR-013 §Consequences and referenced from edge case E8.
- Gap 3: Empty-region behaviour (Q5) was ambiguous between "reserved size still consumed" and "zero budget". Resolved to zero budget in edge case E2 and invariant I3 (centre rect is the exact remainder).
- Gap 4: Hot-reload path (Q6) previously had no failure branch. Added to §2.5 Error Strategy: parse failure on hot-reload logs and keeps old layout.
- Gap 5: Dependency inventory originally implied dockspace holds a tree pointer. Corrected in §2.6: the tree invokes `arrange` on the dockspace; the dockspace does not own a tree reference.
- Gap 6: `extra_regions` extensibility was unbounded. Tightened §2.8: M1 asserts this vector is empty at load time; M9 is the only consumer.

**Checklist result:** 11/11 pass.

### Final

Iteration 1 passes all checklist items. Approach finalised. Implementation may proceed to Phase 2.

---

## 4. Hub Index Update

Add the following row to `docs/technical-approaches/engine/gui.md` Subsystem Index table:

```
| Dockspace | [gui/dockspace.md](dockspace.md) | Generic region allocator: top/bottom/left/right/centre dock regions, config-driven, enforces no-overlap invariant. Replaces per-panel manual rect math. |
```

This leaf is new in M1 but slots in alongside the existing layout-engine and widget-tree leaves — it sits on top of both.
