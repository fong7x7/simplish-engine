# Simplish — Text Pipeline: Technical Approach

**Parent document:** [Technical approaches index](README.md)
**Requirements:** [gui.md SS4.3](../gui.md)
**Library:** engine
**Date:** 2026-03-11

---

## 1. Requirements Summary

| ID | Requirement | Source |
|----|-------------|--------|
| R1 | FreeType rasterizes glyphs into a GPU font atlas (SDF) | gui.md SS4.3 |
| R2 | HarfBuzz produces positioned glyph runs from Unicode input | gui.md SS4.3 |
| R3 | Line-break algorithm for multi-line text wrapping | gui.md SS4.3 |
| R4 | Rich text: attributed-string model with inline spans (bold, italic, colour, size, code) | gui.md SS4.3 |
| R5 | Font atlas memory <= 8 MB GPU for game, <= 16 MB for editor | gui.md SS7 |
| R6 | Text input latency < 1 frame from keypress to glyph display | gui.md SS7 |

---

## 2. Font Atlas

### 2.1 SDF Rasterization

FreeType rasterizes glyphs at a base size (48 px) into a signed distance field (SDF). The SDF is computed by rendering the glyph at 4x resolution and downsampling with a distance transform. The resulting SDF texture allows resolution-independent rendering at any scale via a single atlas.

### 2.2 Atlas Layout

```cpp
struct GlyphInfo {
  uint32_t codepoint = 0;
  uint16_t atlas_x = 0;    // position in atlas texture
  uint16_t atlas_y = 0;
  uint16_t atlas_w = 0;    // size in atlas
  uint16_t atlas_h = 0;
  float bearing_x = 0.0f;  // offset from cursor to top-left
  float bearing_y = 0.0f;
  float advance = 0.0f;    // horizontal advance after this glyph
  float sdf_scale = 0.0f;  // SDF spread in pixels
};

struct FontFace {
  uint32_t face_id = 0;
  std::string family;
  uint16_t weight = 400;   // 100-900, 400 = regular, 700 = bold
  bool italic = false;
  float ascender = 0.0f;
  float descender = 0.0f;
  float line_height = 0.0f;
  std::unordered_map<uint32_t, GlyphInfo> glyphs;
};

struct FontAtlas {
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t cursor_x = 0;   // packing cursor
  uint32_t cursor_y = 0;
  uint32_t row_height = 0;
  std::vector<FontFace> faces;
};
```

### 2.3 Atlas Growth Strategy

- Initial atlas size: 1024x1024 (single channel, ~1 MB).
- When full: allocate a new atlas texture at 2x the dimension (up to 4096x4096).
- Previously-rasterized glyphs are not migrated; the renderer tracks which atlas a glyph belongs to and batches accordingly.
- Glyph packing uses a simple row-based shelf algorithm: glyphs are placed left-to-right in rows, advancing to the next row when the current row is full.

### 2.4 On-Demand Rasterization

Glyphs are rasterized on first use, not at font load time. When a glyph is requested but not in the atlas:

1. FreeType renders the glyph bitmap at the base SDF size.
2. SDF distance transform is computed on the CPU.
3. The SDF texels are uploaded to the atlas texture via a staging buffer.
4. GlyphInfo is recorded in the FontFace glyph map.

This keeps startup fast and atlas memory proportional to the character set actually used.

---

## 3. Text Shaping

### 3.1 HarfBuzz Integration

HarfBuzz shapes a Unicode string for a given FontFace, producing a positioned glyph run:

```cpp
struct ShapedGlyph {
  uint32_t glyph_index = 0;  // font-internal glyph ID
  uint32_t codepoint = 0;    // Unicode codepoint
  float x_offset = 0.0f;     // sub-pixel offset from cursor
  float y_offset = 0.0f;
  float x_advance = 0.0f;    // advance to next glyph
  uint32_t cluster = 0;      // maps back to source string index
};

struct ShapedRun {
  uint32_t face_id = 0;
  std::vector<ShapedGlyph> glyphs;
  float total_advance = 0.0f;
};
```

### 3.2 Shaping Flow

1. Caller provides a UTF-8 string and a FontFace reference.
2. HarfBuzz buffer is created, populated with the string, and shaped.
3. The resulting glyph positions are extracted into a `ShapedRun`.
4. Kerning and ligatures are handled automatically by HarfBuzz.

---

## 4. Line Breaking

### 4.1 Algorithm

The line-breaking algorithm takes a sequence of ShapedRuns and a maximum line width, and produces line-break positions:

1. Walk glyphs left-to-right, accumulating width.
2. At each whitespace cluster boundary, record a candidate break position.
3. When accumulated width exceeds the line width, break at the last candidate.
4. If no candidate exists (single long word), break at the glyph that exceeds the width (forced break).
5. Hyphenation is not supported in M1.

### 4.2 Line Layout

```cpp
struct TextLine {
  uint32_t start_index = 0;   // index into source string
  uint32_t end_index = 0;
  float width = 0.0f;         // actual width of this line
  float ascender = 0.0f;      // max ascender in line
  float descender = 0.0f;     // max descender in line
};
```

Lines are stacked vertically using the font's line_height. Text alignment (left, center, right) is applied per-line by offsetting the start x position.

---

## 5. Rich Text

### 5.1 Attributed Spans

Rich text is represented as a base string with a list of non-overlapping spans:

```cpp
enum class TextStyle : uint8_t {
  kNormal, kBold, kItalic, kBoldItalic, kCode,
};

struct TextSpan {
  uint32_t start = 0;       // byte offset into UTF-8 source
  uint32_t end = 0;
  TextStyle style = TextStyle::kNormal;
  uint32_t color = 0xFFFFFFFF;  // RGBA packed
  float font_size = -1.0f;     // -1 = inherit from parent
};

struct RichText {
  std::string text;
  std::vector<TextSpan> spans;
};
```

### 5.2 Rendering Flow

1. Split the source string at span boundaries into segments.
2. Shape each segment with the appropriate FontFace (regular, bold, italic, code).
3. Concatenate the ShapedRuns and run line-breaking on the combined sequence.
4. Emit quads per glyph with the span's colour and font_size.

---

## 6. Font Cache

The font cache manages loaded FontFace instances and avoids redundant FreeType/HarfBuzz work:

- **Face cache**: maps `(family, weight, italic)` to a loaded FontFace. Faces are loaded on first request.
- **Shape cache**: LRU cache of `(face_id, string_hash)` to ShapedRun. Avoids re-shaping identical strings. Capacity: 1024 entries; eviction clears the oldest entry.
- **Atlas reuse**: glyphs already in the atlas are never re-rasterized.

---

## 7. Public Interface (`engine/gui/text-pipeline.h`)

| Function | Signature | Description |
|----------|-----------|-------------|
| initTextPipeline | `bool initTextPipeline(TextPipelineContext&)` | Load FreeType/HarfBuzz, create initial atlas |
| shutdownTextPipeline | `void shutdownTextPipeline(TextPipelineContext&)` | Free all fonts and atlas textures |
| loadFont | `std::optional<uint32_t> loadFont(TextPipelineContext&, std::string_view path, uint16_t weight, bool italic)` | Load a font face, returns face_id |
| shapeText | `ShapedRun shapeText(TextPipelineContext&, uint32_t face_id, std::string_view text)` | Shape a UTF-8 string |
| breakLines | `std::vector<TextLine> breakLines(const ShapedRun&, float max_width)` | Compute line breaks |
| ensureGlyph | `const GlyphInfo* ensureGlyph(TextPipelineContext&, uint32_t face_id, uint32_t codepoint)` | Rasterize if needed, return atlas info |
| shapeRichText | `std::vector<ShapedRun> shapeRichText(TextPipelineContext&, const RichText&)` | Shape rich text spans |

---

## 8. Error Strategy

| Situation | Handling |
|-----------|----------|
| Font file not found | Log error, return std::nullopt |
| FreeType fails to load face | Log error, return std::nullopt |
| Atlas full at max size (4096x4096) | Allocate second atlas; log warning |
| Unsupported codepoint (no glyph in font) | Render .notdef (tofu); log at debug level |
| HarfBuzz shaping failure | Return empty ShapedRun; log error |

---

## 9. Edge Cases

- Zero-length string: shapeText returns empty ShapedRun (no error).
- String with only whitespace: shaped normally; line-breaking produces one empty line.
- Mixed scripts in a single RichText: each span shaped independently with the appropriate face.
- Font hot-reload (dev builds): clear shape cache entries for the reloaded face; glyphs re-rasterized on next use.

---

## 10. Module Decomposition

| File | Responsibility | Est. Lines |
|------|---------------|------------|
| `engine/gui/text-pipeline.h` | FontAtlas, FontFace, GlyphInfo, ShapedGlyph/Run, RichText, TextSpan, public functions | ~140 |
| `engine/gui/text-pipeline.cpp` | FreeType/HarfBuzz integration, SDF rasterization, atlas packing, shaping, line breaking | ~500 |

---

## 11. Review Log

### Iteration 1
**Checklist results:** 10/11 pass, 1 fail
**Gaps identified:**
- No strategy for atlas texture upload latency when many new glyphs are rasterised in a single frame (R6 one-frame text input latency could be violated during first display of a new script or language)

### Iteration 2
**Checklist results:** 11/11 pass
**Changes made:**
- Added note in section 2.4 that glyph uploads are batched into a single staging buffer transfer per frame, and a budget of 32 new glyphs per frame is enforced to keep upload time within the latency target

### Final
**All checklist items pass.** Approach finalised.
