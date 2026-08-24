# Markdown Table Parser — Technical Approach

> **Parent:** [Technical approaches index](README.md) | **Version:** 1.0 | **Date:** 2026-04-13

**Goal: Extend `MarkdownParser` to recognise GFM-style tables and produce `TABLE` blocks in the AST.**

## 1. Consumers

| Consumer | Usage |
|----------|-------|
| `MarkdownRenderer` | Renders `TABLE` blocks as grid widget subtrees |
| `AiPromptWidget` | No changes — already calls `MarkdownParser::parse()` |

## 2. Context Objects

None — the parser remains a pure stateless transformation.

## 3. New Types

### 3.1 MarkdownColumnAlign

Enum for column alignment derived from the GFM separator row.

| Value | Separator syntax | Rendering |
|-------|-----------------|-----------|
| `LEFT` | `---` or `:---` | Left-aligned text |
| `CENTER` | `:---:` | Center-aligned text |
| `RIGHT` | `---:` | Right-aligned text |

### 3.2 MarkdownTableCell

One cell in a table row. Contains inline spans so cells support bold/italic/code.

| Field | Type | Purpose |
|-------|------|---------|
| `inlines` | `vector<MarkdownInline>` | Formatted content of this cell |

### 3.3 MarkdownTableRow

One row (header or data) in a table.

| Field | Type | Purpose |
|-------|------|---------|
| `cells` | `vector<MarkdownTableCell>` | Cells in this row |

### 3.4 MarkdownTableData

Table-specific metadata attached to a `MarkdownBlock` with type `TABLE`.

| Field | Type | Purpose |
|-------|------|---------|
| `columns` | `vector<MarkdownColumnAlign>` | Per-column alignment from separator row |
| `header` | `MarkdownTableRow` | Header row (always present in valid tables) |
| `rows` | `vector<MarkdownTableRow>` | Data rows (may be empty) |

### 3.5 MarkdownBlockType Extension

Add `TABLE` to the existing enum.

### 3.6 MarkdownBlock Extension

Add an `std::optional<MarkdownTableData> table` field for TABLE blocks.

## 4. Parsing Algorithm

### 4.1 Table Detection (Block Scanner)

A table starts when three consecutive non-blank lines match:
1. **Header row**: contains at least one `|`
2. **Separator row**: matches pattern `|?[\s:]*-{3,}[\s:]*(\|[\s:]*-{3,}[\s:]*)*\|?`
3. **Data row (optional)**: contains at least one `|`

The scanner detects this pattern during Pass 1 (block-level scanning).

### 4.2 Table Parsing Steps

1. **Split header row** by `|`, trim each cell, skip leading/trailing empty cells from outer pipes
2. **Parse separator row** — determine column count and alignment from `:` placement
3. **Parse data rows** — split by `|`, trim, pad short rows with empty cells, truncate excess columns
4. **Parse inline content** — run Pass 2 inline parser on each cell's text
5. **Build `MarkdownTableData`** and wrap in a `MarkdownBlock{TABLE}`

### 4.3 Column Count Resolution

The separator row determines the authoritative column count. Header and data rows are normalised to this count (pad with empty cells or truncate).

### 4.4 Edge Cases

| Case | Behaviour |
|------|-----------|
| No separator row after pipe-containing line | Treat as paragraph |
| Single-column table (`\| val \|`) | Valid table with 1 column |
| Empty cells (`\| \| val \| \|`) | Empty cells produce empty `inlines` vector |
| No data rows (header + separator only) | Valid table with zero data rows |
| Missing outer pipes | Supported (GFM allows omitting outer pipes) |
| Cell with inline formatting | Parsed by existing inline parser |
| Separator row with extra spaces | Trimmed; still valid |
| Escaped pipe in cell content | Not supported (rare in AI responses); `\|` treated as two chars |

## 5. Error Strategy

Malformed tables degrade to paragraphs. If the separator row doesn't parse, the three lines are treated as regular text blocks. This matches the graceful degradation approach of the existing parser.

## 6. Module Decomposition

| File | Responsibility | Est. lines |
|------|---------------|------------|
| `markdown-column-align.h` | `MarkdownColumnAlign` enum | ~20 |
| `markdown-table-cell.h` | `MarkdownTableCell` struct | ~20 |
| `markdown-table-row.h` | `MarkdownTableRow` struct | ~20 |
| `markdown-table-data.h` | `MarkdownTableData` struct | ~25 |
| `markdown-block-type.h` | Add `TABLE` value (modify existing) | +2 lines |
| `markdown-block.h` | Add `optional<MarkdownTableData>` (modify existing) | +3 lines |
| `markdown-parser.cpp` | Table detection + parsing helpers | +~120 lines |

New helper functions in `markdown-parser.cpp` (all ≤16 lines):
- `isTableSeparator` — test if a line is a valid separator row
- `parseAlignment` — extract `MarkdownColumnAlign` from one separator cell
- `parseAlignments` — split separator row into column alignments
- `splitTableRow` — split a pipe-delimited line into cell strings
- `normaliseRowCells` — pad/truncate cells to match column count
- `parseTableBlock` — orchestrate table parsing from raw lines

## 7. Dependencies

Same as existing parser — no new dependencies. `std::optional` for the table field.

## 8. Reuse Analysis

| Existing component | Decision |
|-------------------|----------|
| Existing inline parser (Pass 2) | Reused for cell content — each cell is parsed with the same `parseInlines` |
| `MarkdownBlock` struct | Extended (new optional field), not duplicated |
| `MarkdownBlockType` enum | Extended (new value), not duplicated |

## 9. Open Questions

None.
