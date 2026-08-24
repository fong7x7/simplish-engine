# Markdown Renderer — Technical Approach

> **Parent:** [Technical approaches index](README.md) | **Version:** 1.0 | **Date:** 2026-04-13

**Goal: Convert a `MarkdownBlock` AST into a `GuiWidgetTree` subtree with styled panels and labels, supporting streaming updates.**

## 1. Consumers

| Consumer | Usage |
|----------|-------|
| `AiPromptWidget` | Renders each `AiMessage.content` as a widget subtree |

## 2. Context Objects

### 2.1 MarkdownRenderConfig

Tunable layout and style values for Markdown rendering. Passed as `const&` to the renderer.

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `base_font_size` | `float` | `14.0f` | Default text size in pixels |
| `h1_scale` | `float` | `1.75f` | Heading 1 size multiplier |
| `h2_scale` | `float` | `1.5f` | Heading 2 size multiplier |
| `h3_scale` | `float` | `1.25f` | Heading 3 size multiplier |
| `code_block_padding` | `float` | `8.0f` | Inner padding for code block panels |
| `list_indent` | `float` | `20.0f` | Horizontal indent per list level |
| `blockquote_indent` | `float` | `16.0f` | Horizontal indent for blockquote content |
| `blockquote_border_width` | `float` | `3.0f` | Left border width for blockquotes |
| `block_spacing` | `float` | `6.0f` | Vertical gap between blocks |
| `hr_height` | `float` | `1.0f` | Height of horizontal rule line |
| `text_color` | `GuiColor` | `{220,220,220,255}` | Default text colour |
| `code_bg` | `GuiColor` | `{45,45,45,255}` | Code block background |
| `code_text_color` | `GuiColor` | `{200,200,200,255}` | Code/inline-code text colour |
| `blockquote_border_color` | `GuiColor` | `{100,100,180,255}` | Blockquote left border |
| `blockquote_text_color` | `GuiColor` | `{180,180,200,255}` | Blockquote text colour |
| `link_color` | `GuiColor` | `{100,160,255,255}` | Link text colour |
| `hr_color` | `GuiColor` | `{80,80,80,255}` | Horizontal rule line colour |

### 2.2 MarkdownRenderParams

Groups the parameters for a single render call (keeps function signatures ≤4 params).

| Field | Type | Purpose |
|-------|------|---------|
| `parent_id` | `GuiWidgetId` | Widget under which to create the subtree |
| `config` | `const MarkdownRenderConfig*` | Style/layout configuration |
| `previous_complete_count` | `std::size_t` | Blocks already rendered (0 = full render) |

## 3. Public Interface

```cpp
namespace eng::gui {

struct MarkdownRenderer {
  /// Render markdown blocks as child widgets under params.parent_id.
  /// When params.previous_complete_count > 0, only re-renders from that
  /// block index onward (streaming optimisation).
  /// @thread_safety Main thread only.
  static void render(GuiWidgetTree& tree,
                     const MarkdownRenderParams& params,
                     std::span<const MarkdownBlock> blocks);

  /// Remove all child widgets under the given parent.
  /// @thread_safety Main thread only.
  static void clear(GuiWidgetTree& tree, GuiWidgetId parent_id);
};

}  // namespace eng::gui
```

## 4. Block-to-Widget Mapping

Each `MarkdownBlock` maps to one or more widgets:

| Block Type | Widget Structure | Styling |
|------------|-----------------|---------|
| `PARAGRAPH` | `GuiLabel` with `RichText` spans | Inline bold/italic/code via `TextSpan` |
| `HEADING` | `GuiLabel` with scaled `font_size` | `h1_scale`–`h3_scale` applied to all spans |
| `CODE_BLOCK` | `GuiPanel` (bg) → `GuiLabel` (monospace) | `code_bg` fill, `code_text_color`, `TextStyle::CODE` |
| `BULLET_LIST` | `GuiPanel` (container) → per-item row | Indented; see LIST_ITEM |
| `ORDERED_LIST` | `GuiPanel` (container) → per-item row | Indented; numbered prefix |
| `LIST_ITEM` | `GuiPanel` (row) → `GuiLabel` (bullet/number) + `GuiLabel` (text w/ RichText) | `list_indent` offset; bullet: `"\u2022"`, number: `"N."` |
| `BLOCKQUOTE` | `GuiPanel` (border left) → child block widgets | `blockquote_border_width`, `blockquote_indent` |
| `HORIZONTAL_RULE` | `GuiPanel` (thin line) | `hr_height`, `hr_color` fill |

### Inline-to-TextSpan Mapping

Within text-bearing blocks, `MarkdownInline` elements map to `TextSpan` entries on a `RichText` struct:

| Inline Type | TextSpan fields |
|-------------|----------------|
| `TEXT` | `style = NORMAL`, `color = text_color` |
| `BOLD` | `style = BOLD`, `color = text_color` |
| `ITALIC` | `style = ITALIC`, `color = text_color` |
| `BOLD_ITALIC` | `style = BOLD_ITALIC`, `color = text_color` |
| `INLINE_CODE` | `style = CODE`, `color = code_text_color` |
| `LINK` | `style = NORMAL`, `color = link_color` |

The renderer builds a `RichText` by concatenating inline text and recording byte-offset spans.

## 5. Rendering Algorithm

### 5.1 Full Render (previous_complete_count == 0)

1. Destroy all existing children of `parent_id`
2. For each `MarkdownBlock` in `blocks`:
   a. Call the appropriate `renderXxx` helper (paragraph, heading, code, list, blockquote, HR)
   b. Each helper creates widget(s) under `parent_id`

### 5.2 Streaming Render (previous_complete_count > 0)

1. Destroy the last rendered block's widgets (it may have been incomplete)
2. Re-render from `previous_complete_count - 1` onward
3. Create widgets for any new blocks beyond the previous count

This assumes blocks are append-only during streaming (content grows at the tail). If the entire content changes (e.g., thread switch), `previous_complete_count` is 0 → full re-render.

### 5.3 Helper Decomposition

| Function | Responsibility | Est. lines |
|----------|---------------|------------|
| `render` | Dispatch: full vs. streaming, iterate blocks | ≤16 |
| `clear` | Destroy all children of parent | ≤10 |
| `renderParagraph` | Build RichText from inlines, create GuiLabel | ≤16 |
| `renderHeading` | Like paragraph but with scaled font size | ≤16 |
| `renderCodeBlock` | Create panel + label with CODE style | ≤16 |
| `renderBulletList` | Create container panel, iterate children | ≤16 |
| `renderOrderedList` | Like bullet list with numbered prefixes | ≤16 |
| `renderListItem` | Create row panel + bullet/number label + text label | ≤16 |
| `renderBlockquote` | Create bordered panel, recurse into children | ≤16 |
| `renderHorizontalRule` | Create thin panel with fill | ≤10 |
| `buildRichText` | Convert `vector<MarkdownInline>` → `RichText` | ≤16 |
| `destroyLastBlock` | Find and destroy the last block's root widget | ≤10 |

## 6. Integration with AiPromptWidget

### 6.1 MessageRowIds Changes

Replace `content_label` (single TEXT widget) with:

| Field | Type | Purpose |
|-------|------|---------|
| `content_panel` | `GuiWidgetId` | PANEL container for rendered Markdown |
| `rendered_block_count` | `std::size_t` | Blocks rendered last frame (streaming diff) |

Remove `content_str` — no longer needed (renderer creates its own labels with owned text).

### 6.2 AiPromptWidget Changes

| Method | Change |
|--------|--------|
| `growMessages` | Create `content_panel` (PANEL) instead of `content_label` (TEXT) |
| `updateMessageLabels` | Call `MarkdownParser::parse()` then `MarkdownRenderer::render()` with streaming params |
| `shrinkMessages` | No change — `destroyWidget(row_panel)` already destroys subtree |
| `layoutMessageList` | Content panel height becomes dynamic (sum of block widget heights) |

### 6.3 Streaming Flow

```
AiPromptWidget::updateMessageLabels(tree, state)
  for each message i:
    if content unchanged: skip
    blocks = MarkdownParser::parse(msgs[i].content)
    params = {content_panel, &config, row.rendered_block_count}
    MarkdownRenderer::render(tree, params, blocks)
    row.rendered_block_count = blocks.size()
```

Content change detection: compare `msgs[i].content.size()` with a stored `last_content_size` (cheap for streaming where content only grows).

## 7. Error Strategy

- `render()` guards on `parent_id == GUI_WIDGET_ID_INVALID` — returns immediately
- `config == nullptr` — uses static default config
- Empty `blocks` span — no-op (no widgets created)
- `findWidget` returns nullptr — skip that widget (defensive, matches existing patterns)

## 8. Module Decomposition

| File | Responsibility | Est. lines |
|------|---------------|------------|
| `markdown-render-config.h` | `MarkdownRenderConfig` struct | ~45 |
| `markdown-renderer.h` | `MarkdownRenderer` struct + `MarkdownRenderParams` | ~40 |
| `markdown-renderer.cpp` | All render helpers | ~250 |

## 9. Dependencies

| Dependency | Header | Layer |
|------------|--------|-------|
| `GuiWidgetTree` | `<engine/gui/gui-widget-tree.h>` | Engine/GUI |
| `GuiLabel` | `<engine/gui/gui-label.h>` | Engine/GUI |
| `GuiPanel` | `<engine/gui/gui-panel.h>` | Engine/GUI |
| `GuiColor` | `<engine/gui/gui-color.h>` | Engine/GUI |
| `RichText` | `<engine/gui/rich-text.h>` | Engine/GUI |
| `TextSpan` | `<engine/gui/text-span.h>` | Engine/GUI |
| `MarkdownBlock` | `<engine/gui/markdown-block.h>` | Engine/GUI |

All dependencies are within the same layer (Engine/GUI). No upward dependencies.

## 10. Open Questions

None.

---

## Implementation Checklist

- [ ] **[Infrastructure] Create MarkdownBlockType enum** — Define block-level element discriminator.
      Inputs: None
      Outputs: `MarkdownBlockType` enum used by `MarkdownBlock`
      Refs: Parser §3.1

- [ ] **[Infrastructure] Create MarkdownInlineType enum** — Define inline-level element discriminator.
      Inputs: None
      Outputs: `MarkdownInlineType` enum used by `MarkdownInline`
      Refs: Parser §3.2

- [ ] **[Infrastructure] Create MarkdownInline struct** — Inline span with type, text, and optional URL.
      Inputs: `MarkdownInlineType`
      Outputs: `MarkdownInline` struct used by `MarkdownBlock`
      Refs: Parser §3.3

- [ ] **[Infrastructure] Create MarkdownBlock struct** — Block node with type, inlines, children, and metadata.
      Inputs: `MarkdownBlockType`, `MarkdownInline`
      Outputs: `MarkdownBlock` struct used by parser and renderer
      Refs: Parser §3.4

- [ ] **[Infrastructure] Create MarkdownRenderConfig struct** — Tunable style/layout values for rendering.
      Inputs: `GuiColor`
      Outputs: `MarkdownRenderConfig` struct used by `MarkdownRenderer`
      Refs: Renderer §2.1

- [ ] **[Infrastructure] Create MarkdownRenderParams struct** — Groups render call parameters.
      Inputs: `GuiWidgetId`, `MarkdownRenderConfig`
      Outputs: `MarkdownRenderParams` struct used by `MarkdownRenderer::render()`
      Refs: Renderer §2.2

- [ ] **[Core Algorithm] Implement MarkdownParser block-level scanning** — Line-based scanner that splits input into blocks by type.
      Inputs: `string_view` input
      Outputs: `vector<MarkdownBlock>` with correct types but unparsed inlines
      Refs: Parser §5 Pass 1

- [ ] **[Core Algorithm] Implement MarkdownParser inline parsing** — Character-level scanner for bold/italic/code/links within text blocks.
      Inputs: Raw text string per block
      Outputs: `vector<MarkdownInline>` spans populating each block's `inlines` field
      Refs: Parser §5 Pass 2

- [ ] **[Rendering] Implement buildRichText helper** — Convert `vector<MarkdownInline>` to `RichText` with `TextSpan` entries.
      Inputs: `vector<MarkdownInline>`, `MarkdownRenderConfig` (colors)
      Outputs: `RichText` struct ready for `GuiLabel`
      Refs: Renderer §4 inline mapping

- [ ] **[Rendering] Implement renderParagraph** — Create GuiLabel with RichText spans under parent.
      Inputs: `MarkdownBlock` (PARAGRAPH), `GuiWidgetTree`, parent ID, config
      Outputs: TEXT widget added to tree
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderHeading** — Create GuiLabel with scaled font size.
      Inputs: `MarkdownBlock` (HEADING), config (h1/h2/h3 scales)
      Outputs: TEXT widget with font_size override
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderCodeBlock** — Create panel with background + monospace label.
      Inputs: `MarkdownBlock` (CODE_BLOCK), config (code_bg, code_text_color)
      Outputs: PANEL → TEXT widget subtree
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderBulletList and renderOrderedList** — Create container panels with indented list item rows.
      Inputs: `MarkdownBlock` (BULLET_LIST/ORDERED_LIST), config (list_indent)
      Outputs: PANEL → per-item row subtrees
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderListItem** — Create row with bullet/number prefix + text label.
      Inputs: `MarkdownBlock` (LIST_ITEM), item index, list type
      Outputs: PANEL → prefix label + content label
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderBlockquote** — Create bordered panel with recursive child rendering.
      Inputs: `MarkdownBlock` (BLOCKQUOTE), config (border, indent)
      Outputs: PANEL with left border → child block widgets
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement renderHorizontalRule** — Create thin panel with fill colour.
      Inputs: Config (hr_height, hr_color)
      Outputs: PANEL widget
      Refs: Renderer §4, §5.3

- [ ] **[Rendering] Implement MarkdownRenderer::render dispatch** — Full vs streaming render with block iteration.
      Inputs: `GuiWidgetTree`, `MarkdownRenderParams`, `span<MarkdownBlock>`
      Outputs: Complete widget subtree under parent
      Refs: Renderer §5.1, §5.2

- [ ] **[Rendering] Implement MarkdownRenderer::clear** — Destroy all children of a parent widget.
      Inputs: `GuiWidgetTree`, parent `GuiWidgetId`
      Outputs: All children destroyed
      Refs: Renderer §3

- [ ] **[Integration] Update MessageRowIds** — Replace `content_label` with `content_panel` and `rendered_block_count`.
      Inputs: None
      Outputs: Updated struct for rich text message rows
      Refs: Renderer §6.1

- [ ] **[Integration] Update AiPromptWidget::growMessages** — Create PANEL instead of TEXT for content.
      Inputs: Updated `MessageRowIds`
      Outputs: Message rows with content_panel
      Refs: Renderer §6.2

- [ ] **[Integration] Update AiPromptWidget::updateMessageLabels** — Call parser + renderer with streaming params.
      Inputs: `MarkdownParser`, `MarkdownRenderer`, updated `MessageRowIds`
      Outputs: Rich text rendered in message bubbles
      Refs: Renderer §6.2, §6.3

- [ ] **[Integration] Update AiPromptWidget::layoutMessageList** — Dynamic height for content panels based on rendered block count.
      Inputs: Updated message rows with content_panel
      Outputs: Correct layout with variable-height messages
      Refs: Renderer §6.2
