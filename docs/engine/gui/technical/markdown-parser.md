# Markdown Parser — Technical Approach

> **Parent:** [Technical approaches index](README.md) | **Version:** 1.0 | **Date:** 2026-04-13

**Goal: Parse a Markdown subset into a flat block AST suitable for widget-tree rendering.**

The parser handles the Markdown features common in AI chat responses: bold, italic, inline code, code blocks, headings, lists, blockquotes, links, and horizontal rules. It produces a `vector<MarkdownBlock>` — a flat list of blocks, each containing a vector of inline spans.

## 1. Consumers

| Consumer | Usage |
|----------|-------|
| `MarkdownRenderer` | Converts parsed blocks into `GuiWidgetTree` subtree |
| `AiPromptWidget` | Calls `MarkdownParser::parse()` on each `AiMessage.content` |

## 2. Context Objects

No context objects needed — the parser is a pure stateless transformation (`string_view` in, `vector<MarkdownBlock>` out).

## 3. Types

### 3.1 MarkdownBlockType

Enum discriminating block-level elements.

| Value | Markdown syntax | Notes |
|-------|----------------|-------|
| `PARAGRAPH` | Plain text separated by blank lines | Default block type |
| `HEADING` | `#`, `##`, `###` | `heading_level` field: 1–3 |
| `CODE_BLOCK` | Triple backtick fences | `language` field for syntax hint |
| `BULLET_LIST` | `- item` or `* item` | Contains `children` of `LIST_ITEM` |
| `ORDERED_LIST` | `1. item` | Contains `children` of `LIST_ITEM`; `list_start` field |
| `LIST_ITEM` | Child of list blocks | Contains `inlines` for item text |
| `BLOCKQUOTE` | `> text` | Contains `children` (nested blocks) |
| `HORIZONTAL_RULE` | `---` or `***` or `___` | No content |

### 3.2 MarkdownInlineType

Enum discriminating inline-level elements within a block.

| Value | Markdown syntax | Notes |
|-------|----------------|-------|
| `TEXT` | Plain text | No formatting |
| `BOLD` | `**text**` | Maps to `TextStyle::BOLD` |
| `ITALIC` | `*text*` | Maps to `TextStyle::ITALIC` |
| `BOLD_ITALIC` | `***text***` | Maps to `TextStyle::BOLD_ITALIC` |
| `INLINE_CODE` | `` `code` `` | Maps to `TextStyle::CODE` |
| `LINK` | `text` | `url` field stores href |

### 3.3 MarkdownInline

One inline span within a block.

| Field | Type | Purpose |
|-------|------|---------|
| `type` | `MarkdownInlineType` | Inline element kind |
| `text` | `std::string` | Visible text content |
| `url` | `std::string` | Link URL (empty for non-link types) |

### 3.4 MarkdownBlock

One block-level element. Blocks may have inline content (paragraphs, headings, list items) or child blocks (lists, blockquotes).

| Field | Type | Purpose |
|-------|------|---------|
| `type` | `MarkdownBlockType` | Block element kind |
| `inlines` | `vector<MarkdownInline>` | Inline spans for text-bearing blocks |
| `children` | `vector<MarkdownBlock>` | Nested blocks (lists, blockquotes) |
| `language` | `std::string` | Code block language hint |
| `heading_level` | `uint8_t` | Heading depth (1–3, 0 for non-headings) |
| `list_start` | `uint8_t` | Starting number for ordered lists |

## 4. Public Interface

```cpp
namespace eng::gui {

struct MarkdownParser {
  /// Parse a Markdown string into a flat list of blocks.
  /// @thread_safety Main thread only.
  static std::vector<MarkdownBlock> parse(std::string_view input);
};

}  // namespace eng::gui
```

Single static function. No state, no dependencies, no config. Pure function.

## 5. Parsing Algorithm

Two-pass approach following design principle #1 (simplicity):

### Pass 1: Block-level scanning (line-based)

1. Split input into lines
2. Classify each line by prefix: `#` → heading, `` ``` `` → code fence, `- `/`* ` → bullet, `N. ` → ordered, `> ` → blockquote, `---`/`***`/`___` → HR, blank → block separator
3. Accumulate consecutive same-type lines into blocks
4. Code fences toggle a "in code block" flag — content lines are literal

### Pass 2: Inline parsing (per text-bearing block)

1. Scan character by character through block text
2. Match delimiters: `***` → bold-italic, `**` → bold, `*` → italic, `` ` `` → inline code, `[` → link start
3. Build `MarkdownInline` spans for each run
4. Unmatched delimiters emit as plain text (graceful degradation)

### Edge Cases

| Case | Behaviour |
|------|-----------|
| Empty input | Returns empty vector |
| No Markdown syntax | Single `PARAGRAPH` block with one `TEXT` inline |
| Unclosed `**` or `*` | Treat delimiter as literal text |
| Unclosed code fence | Treat remaining lines as code block content |
| Nested lists | Flatten to single level (AI responses rarely nest deeply) |
| Nested bold/italic | `***text***` → `BOLD_ITALIC`, not nested bold+italic |
| Consecutive blank lines | Collapsed to single block boundary |
| Windows line endings (`\r\n`) | Strip `\r` during line splitting |

## 6. Error Strategy

No error returns — all input produces valid output. Malformed Markdown degrades to plain text (a `PARAGRAPH` with `TEXT` inlines). This follows the principle of simplicity and matches how chat UIs handle partial/streaming content.

## 7. Module Decomposition

| File | Responsibility | Est. lines |
|------|---------------|------------|
| `markdown-block-type.h` | `MarkdownBlockType` enum | ~25 |
| `markdown-inline-type.h` | `MarkdownInlineType` enum | ~25 |
| `markdown-inline.h` | `MarkdownInline` struct | ~25 |
| `markdown-block.h` | `MarkdownBlock` struct | ~30 |
| `markdown-parser.h` | `MarkdownParser` struct (public API) | ~25 |
| `markdown-parser.cpp` | Block scanning + inline parsing | ~250 |

All functions in the `.cpp` will be ≤16 lines (or ≤50 for named algorithms with comment headers). The parse logic decomposes into: `splitLines`, `classifyLine`, `scanBlocks`, `parseInlines`, `parseInlineCode`, `parseBoldItalic`, `parseLink`.

## 8. Dependencies

| Dependency | Header | Purpose |
|------------|--------|---------|
| `<string_view>` | stdlib | Input type |
| `<vector>` | stdlib | Output container |
| `<string>` | stdlib | Owned text in AST nodes |
| `<cstdint>` | stdlib | `uint8_t` for enums |

No engine dependencies. No third-party libraries. Pure C++20.

## 9. Reuse Analysis

| Existing component | Applicability | Decision |
|-------------------|---------------|----------|
| `ExpressionEvaluator` | Has string parsing | Not applicable — expression grammar, not Markdown |
| Third-party MD libs | Would add dependency | Rejected — Markdown subset is small; design principle #1 favours simple custom parser |
| `RichText`/`TextSpan` | Inline formatting model | Reused by renderer, not parser (parser produces AST, renderer maps to spans) |

## 10. Open Questions

None — the Markdown subset is well-defined and the parser has no external dependencies.
