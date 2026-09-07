# Simplish — GUI Framework

**Parent document:** [Engine REQUIREMENTS](../REQUIREMENTS.md)
**Version:** 0.1
**Status:** In Progress
**Last Updated:** 2026-03-10

---

> **Provenance and scope.** This framework, and these documents, came from a 3D voxel engine. Sections 1–8 describe the GUI framework itself and apply unchanged — that code is in `src/engine/gui/` and builds today. **Section 9 is a contract for an editor that does not exist here yet**, and its widget catalogue still reflects the workspaces of a voxel editor (character, dialog, crafting, natural resources). Treat §9 as a starting template to reconcile against [Editor REQUIREMENTS](../../editor/REQUIREMENTS.md) — Level, Encounter, Data, and Assets workspaces — not as an accepted specification. Widgets described anywhere in this document exist only where `src/engine/gui/` implements them; the rest are requirements, not features.

## 1. Overview

The Simplish GUI framework is a **custom retained-mode UI system** rendered through the engine's RHI. It is the **engine-provided UI toolkit** — the single source of all UI primitives (windows, panels, buttons, inputs, dropdowns, sliders, trees, tables, modals, viewports, etc.) for the entire project. Every consumer — the in-game runtime (pause menu, HUD, dialog choices, subtitles, inventory) and the desktop editor (docking workspace, tool panels, property inspectors, AI chat panel) — builds on top of this toolkit. There is no Dear ImGui dependency; the engine owns the full UI stack.

The editor is a **consumer** of the engine GUI toolkit, not a provider. It composes engine-provided widgets into workspaces and panels but does not define its own rendering primitives, hit testing, or input dispatch. All UI capabilities — from basic rectangles and text to complex widgets like property inspectors and node graphs — live in the engine and are available to any application built on Simplish.

The framework renders on every platform the engine targets (macOS, Windows, Linux, PS5, Xbox Series X) using whichever RHI backend is active (Vulkan, DX12, GNM, OpenGL). Because the UI is built on the engine's rendering pipeline, it inherits cross-platform support automatically — the same widget code runs identically on desktop and console.

---

## 2. Goals

- Provide a **single engine-owned UI toolkit** that all applications build on — the game runtime, the editor, dev tools, and any future Simplish application. The engine is the provider; applications are consumers. No application defines its own rendering primitives, hit testing, or input dispatch.
- Render through the engine's **RHI** so the UI works identically across all graphics backends and platforms, including PS5 and Xbox Series X. UI widgets are built on top of the rendering engine, not alongside it — they share the same draw call pipeline, shader infrastructure, and GPU resource management as 3D scene rendering.
- Be **fully customizable** so that every game built on the engine can define its own visual identity — different colour palettes, typography, widget shapes, animations, and layout conventions — without modifying framework code. The engine ships no hardcoded aesthetic; every visual property comes from a swappable theme.
- Support **theme stacking and scoping** so multiple themes can coexist in the same process (e.g. the editor uses a Cursor-inspired dark theme while the game's in-engine menus use a completely different art-directed style).
- Enable **rich text rendering** — markdown, syntax-highlighted code blocks, inline generation indicators, chat bubbles with mixed content — for the editor's AI prompt panel and in-game dialog/subtitle display.
- Provide a **docking layout system** for the editor: panels can be docked, tabbed, floated, split, and serialised as workspace presets.
- Support **gamepad navigation** with focus traversal, d-pad movement, and selection for in-game menus on console.
- Provide an **input layer** built on the engine input system (`engine/input`, not yet written) that supports all input devices — keyboard/mouse (via SDL3), DualSense (PS5), Xbox controllers, and generic gamepads — so that UI code never touches platform-specific input APIs directly.
- Keep the API **data-driven** where possible — widget trees, themes, and layouts can be described in JSON and hot-reloaded in development builds.

---

## 3. Dependencies

| Subsystem | Library | State |
|---|---|---|
| Font loading & rasterization | **FreeType 2** (FTL/GPL) — fetched via CMake `FetchContent` | **Linked.** Glyph rasterization, hinting, subpixel rendering |
| Image decoding | **stb_image** — fetched via CMake `FetchContent` | **Linked.** PNG, JPG, BMP, TGA, GIF for the image loader |
| JSON | **nlohmann_json** — fetched via CMake `FetchContent` | **Linked.** Theme and dockspace config loading |
| Windowing/input glue | **SDL3** — fetched via CMake `FetchContent` | **Linked.** Draw ops and text renderer |
| Text shaping | **HarfBuzz** (MIT) | **Not linked.** FreeType is built with `FT_DISABLE_HARFBUZZ`. Shaping is currently the in-tree `shaped-run` path: kerning and ligatures from FreeType alone, no complex-script support. Add HarfBuzz when a target language needs it |
| Unicode line breaking | **ICU** or a lightweight custom implementation | **Not linked.** Line breaking is the in-tree implementation in the text pipeline |

All other rendering is handled by the engine's RHI (quad batching, texture atlases, signed-distance-field text, rounded-rect shaders).

---

## 4. Architecture

### 4.1 Retained-Mode Widget Tree

Unlike immediate-mode systems, the GUI maintains a persistent widget tree. Widgets are created, updated, and destroyed explicitly. The tree is diffed each frame to determine what needs re-layout and re-render. This enables:

- Smooth animations and transitions (opacity fades, slide-ins, colour lerps)
- Efficient partial re-layout when only a subtree changes
- Accessibility hooks (screen readers can walk the widget tree)

### 4.2 Layout Engine

A **flexbox-style layout engine** computes widget positions and sizes. Supports:

- Row and column flex containers with wrap
- Alignment (start, center, end, stretch, space-between)
- Padding, margin, min/max size constraints
- Scrollable containers with inertial scrolling and scroll bars
- Absolute positioning for overlays and tooltips

### 4.3 Text Pipeline

1. **Font loading** — FreeType rasterizes glyphs into a GPU font atlas (SDF or bitmap depending on size threshold). Multiple fonts/weights loaded simultaneously.
   - **A face must be loaded before any text draws.** `GuiDrawContext::drawText` falls back to one placeholder box per character when `face_id` names no loaded face, and face ids start at 1 — so a client that never loads a font, or reports face 0, renders the entire UI as boxes. `RenderedGameClient` loads one at init and reports its id, which is what makes text appear at all.
   - **Which face** is chosen by `selectGuiUiFont`: a font under `<data_dir>/fonts` wins, so a project shipping its own face gets it everywhere; failing that the first platform-preferred family present (`guiPreferredUiFontFamilies`); failing that whatever was discovered. Discovery accepts `.ttf`, `.otf`, and `.ttc`.
2. **Text shaping** — Positioned glyph runs are produced from Unicode input, handling kerning and ligatures. The shipped path uses FreeType directly (see §3); bidirectional and complex-script text needs HarfBuzz, which is not currently linked.
3. **Line breaking** — A line-break algorithm determines wrap points for multi-line text.
4. **Rich text** — An attributed-string model supports inline spans (bold, italic, colour, font size, code, links). The editor's AI chat panel uses this for markdown rendering, code blocks, and streaming response display.
5. **Text input** — Editable text widgets with cursor positioning, selection, clipboard (copy/paste/cut), undo/redo, and IME support.

Headless captures go through `GuiSoftwareRasterizer`, which paints textured quads as solid boxes unless it is handed the CPU font atlas — pass one and glyphs are sampled per pixel, so a capture taken without a GPU still shows legible text. That is what the editor chrome captures do.

### 4.4 Rendering

The GUI renderer batches draw calls through the engine's RHI:

- **Quad batching** — Coloured, textured, and rounded-rect quads merged into a single vertex buffer per frame where possible.
- **SDF text** — Signed-distance-field rendering for resolution-independent text at arbitrary scale.
- **Clipping** — Scissor-rect stack for scroll containers and overflow. A push both intersects with the enclosing rect and emits a `PUSH_SCISSOR` draw command; the stack on its own is only bookkeeping, and content whose clip never reaches the command stream paints over the rest of the frame. Batching stops at a scissor command, so quads on either side of one cannot merge. The CPU rasterizer (§4.4) reads vertices rather than commands and so does not clip — a headless capture of an overflowing widget shows the overflow.
- **Blur and shadow** — Gaussian blur pass for panel drop shadows and frosted-glass backgrounds (opt-in per panel, with quality tier fallback to solid colour on lower-end hardware).
- **Draw order** — Z-sorted layers: game viewport, HUD, menus/overlays, editor panels, tooltips, modals. A frame may also carry a *scene split*: `markSceneSplit` records the point in the paint order where a depth-tested 3D pass composites, and the client submits the commands before it, then the scene, then the rest. That is what lets ground-plane overlays sit under geometry while the interface stays over it.

### 4.5 Theming & Customization

The GUI framework is **visually game-agnostic** — it ships no hardcoded look and feel. Every visual property is driven by a **theme**, which is a JSON file (or set of JSON files) that any game or application can replace entirely.

#### Theme Definition

A theme JSON defines:

- **Colour palette** — background, surface, primary, accent, text, muted, error, warning, success, and arbitrary named colours. Games can add custom colour tokens beyond the base set.
- **Typography** — font families, weights, and sizes for each semantic role (heading, body, label, caption, code, chat). Games can define additional semantic roles.
- **Shape** — corner radii, border widths, and border styles per widget type. A game could use sharp rectangles everywhere while the editor uses rounded corners.
- **Spacing scale** — a base unit and multiplier ladder for padding, margin, and gap values.
- **Shadows and effects** — drop shadow offsets/blur/colour, frosted-glass blur radius, glow parameters. All optional; a game can disable all effects for a flat look.
- **Transitions** — duration and easing curves for hover, focus, expand/collapse, and fade animations. Set to zero for instant state changes if the game's art direction calls for it.
- **Widget overrides** — per-widget-type style blocks that override the base palette (e.g. a `Button` can have its own corner radius, background gradient, and hover colour distinct from `Panel`).
- **Custom widget styles** — games can register named style classes and apply them to any widget instance, enabling variant looks (e.g. `"style": "inventory-slot"`) without subclassing.

#### Theme Stacking and Scoping

Multiple themes can coexist in the same process:

- **Root theme** — the default theme for the application (set by the game or editor at startup).
- **Scoped theme overrides** — any subtree of the widget tree can push a theme scope that overrides specific tokens. This allows, for example, an in-game terminal panel to use a monospace green-on-black style while the rest of the HUD uses the game's standard theme.
- **Editor vs. game isolation** — the editor applies its own theme (Cursor-inspired dark theme) to all editor panels while the in-editor 3D viewport's HUD preview uses the game's theme, so designers see exactly how their game UI will look.

#### Shipped Themes

The engine repository includes a small set of reference themes under `data/themes/`:

| Theme | Purpose |
|---|---|
| `editor-dark.json` | Cursor-inspired dark theme for the Simplish editor — clean typography, rounded panels, subtle shadows, accent colours for AI interaction states |
| `editor-light.json` | Light variant of the editor theme |
| `game-default.json` | Minimal reference theme for game runtime UI — intended as a starting point that games override with their own art direction |

Games are expected to replace `game-default.json` (or ignore it entirely) with their own theme. The editor themes ship with the editor binary and are not included in game builds.

#### Hot-Reload

Theme JSON files are hot-reloadable in development builds. Changing a theme file on disk immediately re-skins all widgets using that theme within 1 frame, enabling rapid visual iteration.

### 4.6 Docking (Editor Only)

The docking system is an editor-only extension built on top of the core widget tree:

- Panels can be **docked** (left, right, top, bottom, center tab), **floated**, or **hidden**.
- Drag-and-drop panel rearrangement with visual drop-target indicators.
- Layout is serialised to a workspace preset file (JSON) and restored on next launch.
- Workspace presets per editor mode (Map, Character, Item, Dialog, etc.) with user-customisable overrides.

### 4.7 Input

The GUI framework consumes input through the **engine input system** ([input.md](../REQUIREMENTS.md)). It does not poll raw SDL3, ScePad, or GameInput events directly. Instead, the engine input system translates platform-specific raw input into a unified event stream, and the GUI framework receives events through that abstraction. This ensures that the same GUI code works identically across keyboard/mouse (desktop), DualSense (PS5), Xbox controllers, and any future input devices — without platform-specific code in the UI layer.

#### Input Flow

```
Platform Input (SDL3, ScePad, GameInput)
        │
  Engine Input System  ← action map, contexts, dead zones, rebinding
        │
  GuiInputRouter       ← hit testing, focus, event dispatch
        │
  Widget Tree          ← widgets receive typed events
```

The `GuiInputRouter` sits between the engine input system and the widget tree. It performs hit testing against the widget tree to determine which widget receives pointer events, manages keyboard/gamepad focus traversal, and dispatches typed events (click, hover, text, key, scroll, gamepad confirm/cancel) to the appropriate widget.

#### Supported Input Devices

- **Keyboard + Mouse** — Click, double-click, drag, hover, scroll wheel. Hit testing against the widget tree. Focus management, tab order, keyboard shortcuts, text input with IME.
- **Gamepad (Xbox, PlayStation, generic)** — D-pad focus navigation through the widget tree, confirm/cancel button mapping (using the engine input system's canonical button set — see `engine/input` (not yet written)), analog scroll via stick. Used for in-game menus on console and optionally in the editor. Focus traversal follows the widget tree's layout order with spatial navigation for grid layouts.
- **DualSense (PS5)** — Full DualSense support through the engine input system's ScePad backend. Touchpad input forwarded as pointer events. Haptic feedback available through the engine input system's haptic API (`engine/input`, not yet written).
- **Xbox Controller** — Full Xbox controller support through the engine input system's GameInput backend. Impulse trigger feedback available through the haptic API.
- **Touch** (future) — Tap, long-press, swipe. Not in initial scope but the architecture does not preclude it.

#### Input Contexts

The GUI framework registers its own input contexts on the engine input system's context stack (`engine/input`, not yet written). When a modal dialog or menu is open, the GUI pushes an opaque context that blocks input from reaching gameplay. When the GUI has no active focus, it uses a transparent context that passes unhandled input to lower contexts.

#### Input Method Adaptation

The GUI framework listens to the engine input system's `method_changed` event (`engine/input`, not yet written) to switch between mouse-driven and focus-driven interaction. When the player switches from keyboard/mouse to gamepad, the GUI automatically activates focus navigation and shows gamepad-appropriate button prompts (e.g. "A to confirm" vs "Enter to confirm").

---

## 5. Widget Library

### Core Widgets

| Widget | Description |
|---|---|
| `Panel` | Container with optional title bar, background, border, shadow, rounded corners |
| `Text` | Static text label with rich-text attributed spans |
| `TextInput` | Single-line editable text field with placeholder, validation, IME |
| `TextArea` | Multi-line editable text area with line numbers, word wrap, scroll |
| `Button` | Clickable button with label, icon, hover/active/disabled states |
| `Checkbox` | Toggle with label |
| `Slider` | Horizontal/vertical slider with range, step, and value display |
| `Dropdown` | Combo box with searchable option list. Built: item rows with per-row enable/disable, divider rows, and right-aligned accelerator hints, which is what the editor menu bar is assembled from |
| `Tree` | Hierarchical collapsible tree view |
| `Table` | Scrollable table with sortable columns, resizable headers |
| `ScrollContainer` | Scrollable area with vertical/horizontal scroll bars and inertial scrolling |
| `Tabs` | Tab bar with closeable, reorderable tabs |
| `Modal` | Centred overlay dialog with dimmed background; supports confirmation, warning, error, and info variants (§5.1) |
| `Tooltip` | Hover-triggered floating label |
| `Splitter` | Draggable divider between two child regions |
| `Image` | Texture display with aspect ratio modes |
| `Canvas` | Custom draw area — node graphs, timelines, 2D previews |
| `ChatPanel` | Scrollable message list with rich-text bubbles, streaming response indicators, and inline prompt input — purpose-built for the editor's AI assistant panel |

### Editor-Only Widgets

| Widget | Description |
|---|---|
| `DockArea` | Top-level docking container that manages panel arrangement |
| `DockPanel` | Panel with dock handle, tab integration, and serialisable layout position |
| `PropertyInspector` | Auto-generated property grid from reflected C++ types or JSON schemas |
| `AssetBrowser` | File/folder tree + grid thumbnail view with drag-and-drop |
| `NodeGraph` | Zoomable canvas with connectable nodes and bezier links — used by dialog editor, pattern generator, scenario timeline |

### 5.1 Pop-up Dialogs

The framework provides **standard pop-up dialog patterns** built on the `Modal` widget. These cover common user flows in both the game runtime (pause menu, save/load, quit) and the editor (unsaved changes, delete confirmation, error reporting).

#### Dialog Types

| Type | Purpose | Typical use |
|---|---|---|
| **Confirmation** | Obtain explicit user consent before a potentially destructive or irreversible action | Save overwrite, load (unsaved progress lost), delete save, quit without saving |
| **Warning** | Alert the user to consequences or risks before proceeding | "This action cannot be undone", rendering backend switch requires restart |
| **Error** | Report a failure and optionally offer recovery actions | Save failed (disk full), load failed (corrupted save), write error |
| **Info** | Informational message requiring acknowledgment | Operation complete, feature unavailable on current hardware |

#### Requirements

- **Modal behaviour** — Pop-ups block interaction with underlying content until dismissed. The backdrop is dimmed (configurable opacity) and optionally blurred. Clicking the backdrop either dismisses (for non-critical dialogs) or does nothing (for critical confirmations); behaviour is configurable per dialog.
- **Semantic styling** — Each type maps to theme tokens: confirmation uses `primary`/`accent`; warning uses `warning`; error uses `error`; info uses `surface`/`muted`. Optional icon (warning triangle, error cross, info circle) is themeable and can be disabled.
- **Button layouts** — Standard layouts: single **OK** (info, error); **Confirm** / **Cancel** (confirmation); **Yes** / **No** / **Cancel** (multi-choice confirmation); custom button sets for game-specific flows (e.g. **Save & Quit** / **Quit Without Saving** / **Cancel**).
- **Content** — Title (optional), body text (rich-text supported), and optional detail/collapsible section for error messages. All text is localisation-key driven.
- **Input** — Keyboard: `Enter` confirms primary action, `Escape` cancels (when cancel is available). Gamepad: `South` confirms, `East` cancels. Focus is trapped within the dialog; tab order cycles through buttons.
- **Stacking** — Multiple pop-ups can stack (e.g. confirmation over a settings panel). Each layer adds to the dimming; focus moves to the topmost dialog. Draw order follows the existing "modals" layer in §4.4.
- **API** — The framework exposes a C++ API to show dialogs programmatically: `show_confirmation(title_key, body_key, on_confirm, on_cancel)`, `show_warning(...)`, `show_error(...)`, `show_info(...)`, plus a generic `show_modal(content_widget, options)` for custom layouts. Callbacks are invoked on button press; the dialog closes automatically unless the callback returns "keep open".

---

## 6. Integration Points

### Game Runtime

- **Pause menu** — Modal overlay with save/load, settings, controls. See [pause-menu.md](../../game/REQUIREMENTS.md).
- **HUD** — Health, stamina, interaction prompts, awareness indicators, inventory quick-bar.
- **Dialog UI** — NPC dialog choices, subtitles, conversational AI selector wheel (query builder: action + arguments).
- **Dev console** (dev builds only) — Semi-transparent overlay panel. See [dev-console.md](../REQUIREMENTS.md).
- **Audit log viewer** (dev builds only) — Event log panel. See [audit.md](../REQUIREMENTS.md).

### Editor

- **All editor panels** — Map tools, character sculpting, item editing, firearms tuning, dialog graph, AI chat, motion generation, mod workspace. See [editor.md](../../editor/REQUIREMENTS.md).
- **Docking layout** — Workspace presets serialised per editor mode.
- **AI prompt panel** — ChatPanel widget with markdown rendering, streaming response display, inline generation status. See [editor.md §8](../../editor/REQUIREMENTS.md).
- **Editor requirements contract** — §9 below specifies the complete set of primitives, widgets, and behaviours the framework must provide for the editor. Implementers use it as the authoritative checklist when building the editor.

---

## 7. Non-Functional Requirements

| Requirement | Target |
|---|---|
| Draw call budget | ≤ 50 draw calls for a typical in-game HUD; ≤ 200 draw calls for a full editor frame with all panels visible |
| Frame time contribution | < 1 ms CPU + GPU for in-game HUD on all platforms; < 2 ms for editor UI on desktop |
| Font atlas memory | ≤ 8 MB GPU for all loaded fonts at runtime (game); ≤ 16 MB for the editor (more fonts/sizes) |
| Text input latency | < 1 frame of latency from keypress to glyph display |
| Theme hot-reload | Theme JSON changes reflected within 1 frame in dev builds |

---

## 8. Milestones

The GUI framework is engine infrastructure that ships before both the game runtime UI and the editor:

| Milestone | Scope |
|---|---|
| M1 | Core widget tree, layout engine, text pipeline (FreeType + HarfBuzz), quad-batch RHI renderer, basic widgets (Panel, Text, Button, TextInput, ScrollContainer), dark theme, dev console overlay |
| M7 (with game systems) | Full game runtime UI: pause menu, HUD, dialog choice UI, subtitle rendering, gamepad navigation, conversational AI selector wheel (query builder) |
| M9 (with editor) | Docking system, editor-only widgets (DockArea, PropertyInspector, AssetBrowser, NodeGraph, ChatPanel), workspace presets, rich-text markdown rendering, blur/shadow polish |

---

## 9. Editor Requirements (Contract for Building the Editor)

The editor is built entirely on this GUI framework. This section specifies the **complete set of primitives, widgets, and behaviours** the framework must provide so the editor can implement all workspaces, panels, and tools described in [editor.md](../../editor/REQUIREMENTS.md) and its sub-documents. Implementers use this as the authoritative checklist.

### 9.1 Editor Shell Primitives

| Primitive | Description | Editor Use |
|---|---|---|
| **MenuBar** | Horizontal menu bar at top of window. Supports nested menus, items with labels and shortcuts, separators, disabled items, checkable items (toggle state). Plugins can contribute menu items via manifest. Not yet an engine widget: the editor builds its own bar out of `Button` and `Dropdown` (`src/editor/shell/editor-menu-bar-widget.h`), which is the working reference for what this one has to generalise — nesting, checkable items, and plugin contribution are the gaps. | File, Edit, View, Map, Character, Item, Window, Help |
| **Toolbar** | Horizontal strip of icon buttons with optional labels. Supports grouping (separators), tooltips, disabled state, toggle (pressed) state. Per-workspace toolbar content. Plugins contribute toolbar items. | Level tools (tile paint, height, fill, select), prop and entity placement, encounter tools |
| **StatusBar** | Horizontal strip at bottom of window. Left/center/right regions. Supports text, icons, progress indicators, clickable segments. Plugins contribute status bar items. | Selection count, coordinates, dirty indicator, plugin status |
| **SimulationStrip** | Compact horizontal strip for Play/Pause/Stop/Step. Visible in editor header. | Play-test controls |

### 9.2 Core Widgets (Editor Use)

All widgets in §5 must support the following for editor use:

| Widget | Editor Requirements |
|---|---|
| **Panel** | Title bar (optional), collapse/expand, close button, context menu. Must support being a dock target. |
| **Text** | Rich-text spans, truncation with ellipsis, selection (for copy). |
| **TextInput** | Placeholder, validation state (error/warning outline), IME, undo within field. |
| **TextArea** | Line numbers (optional), word wrap toggle, syntax highlighting (for raw JSON view). |
| **Button** | Icon + label, icon-only, disabled, loading state (spinner). |
| **Checkbox** | Indeterminate state (for batch selection). |
| **Slider** | Value display (optional), unit suffix (m, °), logarithmic scale (optional). |
| **Dropdown** | Search/filter, custom item renderer (e.g. tile swatch + id), multi-select variant. |
| **Tree** | Checkboxes, drag-and-drop reorder, context menu per node, lazy loading. |
| **Table** | Sortable columns, resizable columns, row selection (single/multi), context menu, inline editing. |
| **Tabs** | Close button per tab, reorderable, overflow menu when tab bar is full. |
| **ScrollContainer** | Inertial scrolling, scroll bar visibility (auto/always/never), programmatic scroll. |
| **Splitter** | Nested splitters, minimum size constraints, serialised split ratio. |
| **Tooltip** | Rich-text content, delay, max width. |
| **Modal** | Per §5.1; must support custom content widget (e.g. file picker, id prompt). |

### 9.3 Editor-Specific Widgets

| Widget | Description | Used By |
|---|---|---|
| **DockArea** | Top-level docking container. Manages panel arrangement, drag-and-drop reorder, tab groups, float windows. Serialises layout to JSON. | Editor shell |
| **DockPanel** | Panel with dock handle, tab integration, serialisable position. | All editor panels |
| **PropertyInspector** | Auto-generated property grid from JSON Schema or reflected C++ types. Renders form from schema with type-appropriate widgets. Supports batch editing (mixed-value indicator). Collapsible sections. | All workspaces |
| **AssetBrowser** | Two-pane: folder tree (left) + grid/list (right). Thumbnails for meshes, textures, prefabs, presets. Drag-and-drop to viewport or other panels. Search, filter by type. Context menu (Open, Show in Editor, etc.). | All workspaces |
| **NodeGraph** | Zoomable/pannable canvas. Nodes with input/output ports. Bezier connections. Selection, multi-select, box select. Copy/paste nodes. Context menu. Used for dialog trees, particle emitter graph, animation state machine, scenario timeline. | Dialog, Particle, Animation, Scenario |
| **ChatPanel** | Message list with rich-text bubbles. Streaming response indicator. Inline prompt input. @ mention autocomplete. | AI Prompt Panel |
| **CurveEditor** | 2D canvas with editable curve (keyframes, tangents). Used for particle lifetime, falloff, time-of-day curves. | Particle, Lighting, Data |
| **ColourPicker** | Hue/saturation/value or RGB sliders. Swatch preview. Alpha channel (optional). Hex input. | Data editors, Preferences |
| **AssetPathPicker** | Text input + browse button. Opens Asset Browser filtered by type. Validates path exists. | Data editors, all asset references |
| **EntityRefPicker** | Dropdown populated from Entity Catalog. Search by name/id. Shows entity type icon. | Map, Scenario |
| **TilePicker** | Dropdown with the tileset palette. Thumbnail per tile type. Search. Multi-select variant for brush and spawn rules. | Level, Data |
| **ItemRefPicker** | Dropdown populated from item definitions. Search. Icon/thumbnail. | Crafting, Data |
| **Minimap** | 2D top-down view of map region. Pan/zoom. Player/camera position indicator. Click to teleport camera. | Map workspace |
| **PhonemeTimeline** | Horizontal timeline with phoneme blocks. Scrubbing. Viseme thumbnails. | Dialog workspace |
| **EventTrackEditor** | Timeline with tracks. Keyframe markers. Add/remove/move keyframes. | Animation workspace |
| **ValidationStrip** | Compact strip showing error/warning count. Click to open Validation Panel. Inline error icons. | Data, List/Inspector editors |

### 9.4 Embedded RHI Viewport

The editor embeds live viewports (the isometric level view, sprite preview, prop preview) inside panels. The framework must provide:

| Requirement | Description |
|---|---|
| **ViewportHost** | A widget that reserves a rectangular region and yields an RHI render target. The engine renders the scene into this region. The widget handles resize, aspect ratio, and input forwarding (mouse, keyboard) to the viewport controller. |
| **Input routing** | Mouse events (click, drag, scroll) and keyboard events are routed to the focused viewport. The widget tree performs hit testing to determine which viewport receives input when multiple viewports exist. |
| **Overlay layer** | The viewport can have an overlay widget tree (gizmos, grid, minimap, tool buttons) rendered on top of the 3D content. The framework composites the overlay and the scene in the correct order. |

### 9.5 Notifications and Feedback

| Primitive | Description | Editor Use |
|---|---|---|
| **Toast** | Non-blocking notification that appears briefly (e.g. bottom-right). Severity: info, success, warning, error. Auto-dismiss (configurable duration) or dismiss on click. Stack multiple toasts. | "Saved", "Compiled 3 assets", "Plugin failed to load" |
| **Progress** | Blocking or non-blocking progress indicator. Progress bar with percentage, optional message, Cancel button. Used for import, batch export, migration. | Asset import, Batch Export, Save All |
| **Spinner** | Indeterminate progress (loading state). Inline in button or as overlay. | AI generation, loading |

### 9.6 Context Menus and Drag-and-Drop

| Feature | Description |
|---|---|
| **Context menu** | Right-click triggers a pop-up menu. Menu content is context-dependent (selection type, panel, workspace). Same structure as MenuBar items (nested menus, shortcuts, separators). Dismiss on click outside or Escape. |
| **Drag-and-drop** | Widgets can be drag sources and/or drop targets. Drag initiates on mouse down + move threshold. Visual drag preview (optional). Drop target highlight on hover. MIME-type or custom data for cross-panel D&D. Used for: Asset Browser → Viewport (place prefab), Asset Browser → Inspector (assign reference), List → List (reorder), NodeGraph (connect nodes). |

### 9.7 Search and Filter

| Feature | Description |
|---|---|
| **SearchBox** | Text input with clear button, placeholder "Search…". Optional filter dropdown (e.g. "All types", "Entities only"). Debounced search callback. |
| **Fuzzy matching** | Framework provides fuzzy substring matching for Command Palette, dropdowns, and search. Score by contiguity, prefix, word boundary. |
| **Filter chips** | Optional filter UI: clickable chips (e.g. "Errors only", "Warnings") that toggle filter state. |

### 9.8 Validation and Error Display

| Feature | Description |
|---|---|
| **Error outline** | Widget can display a red outline to indicate validation error. Theme token `error` or `validation_error`. |
| **Warning outline** | Yellow/amber outline for warnings. Theme token `warning`. |
| **Error icon + tooltip** | Small icon (e.g. triangle) next to field. Hover shows error message. |
| **Validation panel** | Scrollable list of validation issues. Each entry: severity icon, message, path/field, click to navigate. |

### 9.9 Schema-Driven Form Rendering

The PropertyInspector and Data workspace form view render UI from JSON Schema. The framework must support:

| Schema → Widget Mapping | Implementation |
|---|---|
| `type: string` | TextInput (or TextArea if `maxLength` > 80) |
| `type: string`, `enum` | Dropdown |
| `type: string`, `format: uri` or asset path | AssetPathPicker |
| `type: number` / `type: integer` | Slider (if min/max/step) or numeric TextInput |
| `type: boolean` | Checkbox |
| `type: array` | List with add/remove/reorder; each item rendered per `items` schema |
| `type: object` | Collapsible section; nested properties rendered recursively |
| `x-editor-widget: "colour"` | ColourPicker |
| `x-editor-widget: "curve"` | CurveEditor |
| `x-editor-widget: "asset-path"` | AssetPathPicker |
| `x-editor-widget: "entity-ref"` | EntityRefPicker |
| `x-editor-widget: "tile-type"` | TilePicker |
| `x-editor-group` | Collapsible section header |

Required/optional indicators, default value display, and description tooltips are rendered from schema `required`, `default`, and `description` fields.

### 9.10 Command Palette Overlay

| Requirement | Description |
|---|---|
| **Overlay** | Full-screen dimmed overlay with centred search box. Steals focus. Dismiss on Escape or click outside. |
| **Search input** | Single-line text input. Prefix characters (`>`, `@`, `#`, `:`) switch mode. |
| **Result list** | Scrollable list below search. Fuzzy-matched results. Grouped by category. Keyboard navigation (Up/Down, Enter to execute). |
| **Preview** | Optional preview pane (e.g. entity thumbnail, command description) when result is selected. |

### 9.11 Localisation and Accessibility

| Requirement | Description |
|---|---|
| **Localisation** | All user-facing strings are localisation-key driven. The framework resolves keys at render time. Fallback to key if translation missing. |
| **Accessibility** | Widget tree is traversable by screen readers. Labels associated with inputs. Focus order follows visual layout. High-contrast theme support. |

### 9.12 Editor Requirements Summary

The following table maps editor subsystems to GUI requirements. Each must be satisfiable by the framework.

| Editor Subsystem | GUI Requirements |
|---|---|
| Editor shell | MenuBar, Toolbar, StatusBar, SimulationStrip, DockArea, DockPanel |
| Map workspace | ViewportHost, Toolbar, PropertyInspector, Tree (layer panel), Minimap, VoxelTypePicker, EntityRefPicker |
| Character workspace | ViewportHost, Tabs, Slider, PropertyInspector, ColourPicker |
| Item workspace | ViewportHost, Toolbar, PropertyInspector, AssetPathPicker |
| Data workspace | PropertyInspector (schema-driven), SearchBox, Tree, Table, ValidationStrip, ColourPicker, AssetPathPicker, EntityRefPicker, VoxelTypePicker, CurveEditor |
| Dialog workspace | NodeGraph, ViewportHost (character preview), PhonemeTimeline, ChatPanel |
| Scenario workspace | NodeGraph (timeline), EventTrackEditor, EntityRefPicker |
| Particle workspace | NodeGraph (emitter graph), CurveEditor, ViewportHost |
| Animation workspace | NodeGraph (state machine), EventTrackEditor, ViewportHost |
| List/Inspector editors | Tree, Table, PropertyInspector, Context menu, ValidationStrip |
| Asset Browser | AssetBrowser widget, Drag-and-drop |
| Command Palette | Command Palette Overlay |
| AI Prompt Panel | ChatPanel |
| Preferences | Modal, Slider, Dropdown, ColourPicker, AssetPathPicker |
| Notifications | Toast, Progress, Modal (errors) |

---

*This document is a living spec. Update it as GUI framework decisions are made and requirements evolve.*
