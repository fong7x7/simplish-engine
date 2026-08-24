# Markdown Table Renderer — Technical Approach

> **Parent:** [Technical approaches index](README.md) | **Version:** 1.0 | **Date:** 2026-04-13

**Goal: Render `TABLE` blocks from the Markdown AST as grid-layout widget subtrees with styled headers, alignment, and horizontal scrolling.**

## 1. Consumers

| Consumer | Usage |
|----------|-------|
| `MarkdownRenderer` | Dispatches `TABLE` blocks to `renderTable` helper |
| `AiPromptWidget` | No changes — renderer integration already exists |

## 2. Architecture

```
MarkdownBlock (TABLE)
    │
    ▼
renderTable(tree, parent, block, config)
    │
    ├── SCROLL_CONTAINER (horizontal scroll when table > parent width)
    │     │
    │     └── PANEL (table_body, COLUMN layout)
    │           │
    │           ├── PANEL (header_row, ROW layout, distinct bg)
    │           │     ├── PANEL (cell_0) → GuiLabel (RichText, bold)
    │           │     ├── PANEL (cell_1) → GuiLabel (RichText, bold)
    │           │     └── ...
    │           │
    │           ├── PANEL (data_row_0, ROW layout, even bg)
    │           │     ├── PANEL (cell_0) → GuiLabel (RichText)
    │           │     └── ...
    │           │
    │           ├── PANEL (data_row_1, ROW layout, odd bg)
    │           │     └── ...
    │           └── ...
```

## 3. Config Extensions to MarkdownRenderConfig

| Field | Type | Default | Purpose |
|-------|------|---------|---------|
| `table_header_bg` | `GuiColor` | `{50,55,65,255}` | Header row background |
| `table_even_row_bg` | `GuiColor` | `{35,38,45,255}` | Even data row background |
| `table_odd_row_bg` | `GuiColor` | `{40,43,52,255}` | Odd data row background |
| `table_border_color` | `GuiColor` | `{65,70,80,255}` | Cell border colour |
| `table_cell_padding` | `float` | `6.0f` | Inner padding per cell |
| `table_border_width` | `float` | `1.0f` | Border width between cells |
| `table_min_column_width` | `float` | `40.0f` | Minimum column width |
| `table_max_column_width` | `float` | `300.0f` | Maximum column width |

## 4. Rendering Algorithm

### 4.1 renderTable (entry point, ≤16 lines)

1. Guard: if `block.table` is `nullopt`, return
2. Create scroll container under parent (horizontal scroll enabled)
3. Create table body panel (COLUMN layout) inside scroll container
4. Call `renderTableHeader` for the header row
5. Call `renderTableRows` for data rows

### 4.2 renderTableHeader (≤16 lines)

1. Create row panel (ROW layout, `table_header_bg` fill)
2. For each cell in `header.cells`: call `renderTableCell` with bold style flag

### 4.3 renderTableRows (≤16 lines)

1. Iterate data rows
2. For each row: create row panel with alternating `table_even_row_bg`/`table_odd_row_bg`
3. For each cell: call `renderTableCell`

### 4.4 renderTableCell (≤16 lines)

1. Create cell panel with `table_cell_padding`, `table_border_width`, `table_border_color`
2. Set cell width constraints (`table_min_column_width`, `table_max_column_width`)
3. Create GuiLabel with RichText built from cell's `inlines`
4. Apply alignment from `MarkdownColumnAlign` → `GuiLabelAlign` mapping

### 4.5 Alignment Mapping

| `MarkdownColumnAlign` | `GuiLabelAlign` |
|-----------------------|-----------------|
| `LEFT` | `LEFT` |
| `CENTER` | `H_CENTER` |
| `RIGHT` | (custom: right-align via layout `justify_content = END`) |

For right-alignment, the cell panel uses `justify_content = Align::END` so the label floats right. LEFT and CENTER use `GuiLabelAlign` directly.

### 4.6 Column Width Strategy

Cells use `flex_grow = 1.0f` with `min_width = table_min_column_width` and `max_width = table_max_column_width`. This distributes space evenly while respecting bounds. The scroll container enables horizontal scrolling when total min-width exceeds parent width.

### 4.7 Text Wrapping

Cell labels are TEXT_AREA widgets (not plain labels) when content exceeds `table_max_column_width`, enabling word wrap within cells. For short content, a GuiLabel suffices. The renderer checks content length and picks the appropriate widget type.

Simplification (design principle #1): always use GuiLabel. The `max_width` constraint on the cell panel combined with the layout engine's text measurement handles truncation. If text wrapping becomes necessary in practice, it can be added later.

## 5. Edge Cases

| Case | Handling |
|------|----------|
| Single-column table | One cell per row; scroll container unnecessary but harmless |
| Empty cells | Cell panel created with no label (or empty-text label) |
| Many columns (>10) | Horizontal scroll activates; cells at `table_min_column_width` |
| Very long cell text | Constrained by `table_max_column_width`; text truncates |
| Table with only header | Body has zero data rows; header renders alone |
| Inline formatting in cells | `buildRichText` (existing helper) handles bold/italic/code spans |

## 6. Streaming Compatibility

Tables are complete blocks — they don't stream incrementally. The existing streaming renderer destroys and re-renders the last block. A TABLE block in-progress (missing closing rows) will be re-parsed and re-rendered each update. This is acceptable because:
- Table parsing is fast (line splitting + inline parsing)
- Table widget creation is bounded by row × column count
- Streaming tables is uncommon (most AI responses complete tables quickly)

## 7. Module Decomposition

| File | Responsibility | Est. lines |
|------|---------------|------------|
| `markdown-render-config.h` | Add 8 table style fields (modify existing) | +10 lines |
| `markdown-renderer.cpp` | Add `renderTable`, `renderTableHeader`, `renderTableRows`, `renderTableCell` | +~80 lines |

No new files for the renderer — all table rendering helpers are static functions in the existing `markdown-renderer.cpp`.

## 8. Dependencies

Same as existing renderer — no new dependencies. Reuses `GuiPanel`, `GuiLabel`, `RichText`, `TextSpan`, `LayoutStyle`, `buildRichText`.

## 9. Reuse Analysis

| Existing component | Decision |
|-------------------|----------|
| `buildRichText` helper | Reused for cell content rendering |
| `MarkdownRenderer::render` dispatch | Extended with TABLE case |
| `GuiPanel` | Reused for row and cell containers |
| `GuiLabel` | Reused for cell text |
| `SCROLL_CONTAINER` widget type | Reused for horizontal overflow |
| `LayoutStyle` flex properties | Reused for column sizing |

## 10. Open Questions

None.

---

## Implementation Checklist

- [ ] **[Infrastructure] Add MarkdownColumnAlign enum** — Column alignment discriminator from separator row.
      Inputs: None
      Outputs: `MarkdownColumnAlign` enum used by `MarkdownTableData`
      Refs: Table Parser §3.1

- [ ] **[Infrastructure] Add MarkdownTableCell struct** — Cell with inline spans.
      Inputs: `MarkdownInline`
      Outputs: `MarkdownTableCell` struct used by `MarkdownTableRow`
      Refs: Table Parser §3.2

- [ ] **[Infrastructure] Add MarkdownTableRow struct** — Row containing cells.
      Inputs: `MarkdownTableCell`
      Outputs: `MarkdownTableRow` struct used by `MarkdownTableData`
      Refs: Table Parser §3.3

- [ ] **[Infrastructure] Add MarkdownTableData struct** — Table metadata with columns, header, rows.
      Inputs: `MarkdownColumnAlign`, `MarkdownTableRow`
      Outputs: `MarkdownTableData` struct used by `MarkdownBlock`
      Refs: Table Parser §3.4

- [ ] **[Infrastructure] Extend MarkdownBlockType with TABLE** — Add TABLE value to existing enum.
      Inputs: None
      Outputs: TABLE block type recognised by parser and renderer
      Refs: Table Parser §3.5

- [ ] **[Infrastructure] Extend MarkdownBlock with table field** — Add `optional<MarkdownTableData>` field.
      Inputs: `MarkdownTableData`
      Outputs: TABLE blocks carry structured table data
      Refs: Table Parser §3.6

- [ ] **[Infrastructure] Extend MarkdownRenderConfig with table styles** — Add 8 table-specific fields.
      Inputs: `GuiColor`
      Outputs: Configurable table appearance
      Refs: Table Renderer §3

- [ ] **[Core Algorithm] Implement table detection in block scanner** — Detect header + separator + data row patterns.
      Inputs: Line array from Pass 1
      Outputs: TABLE blocks identified during scanning
      Refs: Table Parser §4.1

- [ ] **[Core Algorithm] Implement table row parsing helpers** — Split pipe-delimited rows, parse alignments, normalise cell counts.
      Inputs: Raw line strings
      Outputs: `MarkdownTableData` with parsed cells
      Refs: Table Parser §4.2, §4.3

- [ ] **[Core Algorithm] Implement cell inline parsing** — Run existing inline parser on each cell's text content.
      Inputs: Cell text strings
      Outputs: `MarkdownTableCell` with populated `inlines`
      Refs: Table Parser §4.2 step 4

- [ ] **[Rendering] Implement renderTable entry point** — Create scroll container + table body + dispatch to header/rows.
      Inputs: `MarkdownBlock` (TABLE), `GuiWidgetTree`, parent ID, config
      Outputs: Complete table widget subtree
      Refs: Table Renderer §4.1

- [ ] **[Rendering] Implement renderTableHeader** — Create header row with distinct background and bold text.
      Inputs: `MarkdownTableRow`, config, tree
      Outputs: Header row panel with styled cells
      Refs: Table Renderer §4.2

- [ ] **[Rendering] Implement renderTableRows** — Create data rows with alternating backgrounds.
      Inputs: `vector<MarkdownTableRow>`, config, tree
      Outputs: Data row panels with cells
      Refs: Table Renderer §4.3

- [ ] **[Rendering] Implement renderTableCell** — Create cell panel with label, alignment, and width constraints.
      Inputs: `MarkdownTableCell`, `MarkdownColumnAlign`, config, tree
      Outputs: Cell panel with aligned RichText label
      Refs: Table Renderer §4.4, §4.5

- [ ] **[Rendering] Add TABLE dispatch to MarkdownRenderer::render** — Route TABLE blocks to renderTable.
      Inputs: Existing render dispatch logic
      Outputs: TABLE blocks rendered alongside other block types
      Refs: Table Renderer §4.1

- [ ] **[Integration] Verify streaming compatibility** — Ensure TABLE blocks re-render correctly during streaming.
      Inputs: Existing streaming render logic
      Outputs: Tables render correctly during token-by-token updates
      Refs: Table Renderer §6
