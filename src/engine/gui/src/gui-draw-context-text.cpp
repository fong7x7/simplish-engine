/// @file gui-draw-context-text.cpp
/// @brief `GuiDrawContext`'s text: setting, measuring, wrapping and
/// ellipsizing UTF-8 in any `GuiFont`.

#include "utf8-step.h"

#include <algorithm>
#include <cmath>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/text-pipeline.h>
#include <utility>

namespace eng {

namespace {

  /// Width of a placeholder box per character at the default size, when
  /// there is no font to draw with.
  constexpr float PLACEHOLDER_W = 8.0f;
  /// "…", and three dots where the font has no ellipsis.
  constexpr std::string_view ELLIPSIS = "\xE2\x80\xA6";
  constexpr std::string_view DOTS = "...";
  constexpr uint32_t ELLIPSIS_CODEPOINT = 0x2026u;

  /// Everything needed to set text in one font.
  struct Setter {
    /// The glyph source, or null to set placeholder boxes.
    TextPipelineContext* pipeline = nullptr;
    /// The face.
    uint32_t face = 0;
    /// The font.
    GuiFont font{};
    /// Every digit's advance when digits are tabular; 0 otherwise.
    float tabular = 0.0f;
  };

  /// One character as set: where it starts, how far it moves the pen, and
  /// its glyph (null for a placeholder).
  struct SetChar {
    /// Byte offset in the text.
    std::size_t at = 0;
    /// Its codepoint.
    uint32_t codepoint = 0;
    /// Pen position before it, from the start of the text.
    float x = 0.0f;
    /// How far it moves the pen, letter spacing aside.
    float advance = 0.0f;
    /// Its glyph, or null.
    const GlyphInfo* glyph = nullptr;
  };

  bool isDigit(uint32_t cp) {
    return cp >= '0' && cp <= '9';
  }

  const GlyphInfo* glyphOf(const Setter& s, uint32_t cp) {
    return s.pipeline == nullptr
               ? nullptr
               : s.pipeline->ensureGlyph(s.face, cp, s.font.size);
  }

  float placeholderAdvance(const GuiFont& font) {
    return PLACEHOLDER_W * font.size / GUI_DEFAULT_TEXT_SIZE;
  }

  /// The widest digit's advance, for tabular figures.
  float widestDigit(const Setter& s) {
    float widest = 0.0f;
    for (uint32_t cp = '0'; cp <= '9'; ++cp) {
      const GlyphInfo* g = glyphOf(s, cp);
      widest = std::max(widest,
                        g != nullptr ? g->advance : placeholderAdvance(s.font));
    }
    return widest;
  }

  Setter setterFor(const GuiDrawContext& ctx, const GuiFont& font) {
    Setter s{
        .pipeline = ctx.text_pipeline, .face = ctx.faceFor(font), .font = font};
    const bool has_face =
        s.pipeline != nullptr &&
        s.pipeline->metrics(s.face, font.size).line_height > 0;
    s.pipeline = has_face ? s.pipeline : nullptr;
    if (font.digits == GuiDigits::TABULAR) {
      s.tabular = widestDigit(s);
    }
    return s;
  }

  float advanceOf(const Setter& s, uint32_t cp, const GlyphInfo* g) {
    if (s.tabular > 0.0f && isDigit(cp)) {
      return s.tabular;
    }
    return g != nullptr ? g->advance : placeholderAdvance(s.font);
  }

  /// Every character of @p text as @p s sets it, in order.
  std::vector<SetChar> setText(const Setter& s, std::string_view text) {
    std::vector<SetChar> out;
    float pen = 0.0f;
    const GlyphInfo* prev = nullptr;
    for (std::size_t at = 0; at < text.size();) {
      const Utf8Step step = decodeUtf8(text, at);
      const GlyphInfo* g = glyphOf(s, step.codepoint);
      if (prev != nullptr && g != nullptr) {
        pen += s.pipeline->kerning(s.face, *prev, *g);
      }
      out.push_back(
          {at, step.codepoint, pen, advanceOf(s, step.codepoint, g), g});
      pen += out.back().advance + s.font.letter_spacing;
      prev = g;
      at += step.length;
    }
    return out;
  }

  /// Width of @p chars[first, last): to the end of the last's advance.
  float spanWidth(const std::vector<SetChar>& chars, std::size_t first,
                  std::size_t last) {
    if (last <= first) {
      return 0.0f;
    }
    return chars[last - 1].x + chars[last - 1].advance - chars[first].x;
  }

  /// Where the next line starts after breaking before @p i: past any
  /// spaces there.
  std::size_t skipSpaces(const std::vector<SetChar>& chars, std::size_t i) {
    while (i < chars.size() && chars[i].codepoint == ' ') {
      ++i;
    }
    return i;
  }

  /// A line being drawn, and its baseline.
  struct LinePen {
    /// The line.
    const GuiTextDraw& draw;
    /// Its baseline's y.
    float baseline = 0.0f;
  };

  /// Breaks set text into lines, greedily: a line ends at the last space
  /// that keeps it within the width, else before the character that
  /// overflows it, and always at a newline.
  struct LineBreaker {
    /// The text's characters as set.
    std::vector<SetChar> chars;
    /// The text.
    std::string_view text;
    /// Lines so far.
    std::vector<GuiWrappedLine> lines{};
    /// Index of the current line's first character.
    std::size_t start = 0;
    /// Index of the last space in the current line to break at; 0 for none.
    std::size_t space = 0;

    /// Byte offset of character @p i, or the text's end.
    std::size_t byteAt(std::size_t i) const {
      return i < chars.size() ? chars[i].at : text.size();
    }

    /// End the line before character @p end; start the next at @p next,
    /// past any spaces.
    void close(std::size_t end, std::size_t next) {
      lines.push_back(
          {byteAt(start), byteAt(end), spanWidth(chars, start, end)});
      start = skipSpaces(chars, next);
      space = 0;
    }

    /// Take character @p i into the line, breaking it first when it has
    /// to be.
    void consider(std::size_t i, float width) {
      if (chars[i].codepoint == '\n') {
        close(i, i + 1);
      } else if (chars[i].codepoint == ' ' && i > start) {
        space = i;
      } else if (i > start && spanWidth(chars, start, i + 1) > width) {
        const std::size_t at = space > start ? space : i;
        close(at, at);
      }
    }

    /// The lines, the last one included.
    std::vector<GuiWrappedLine> finish() {
      if (start < chars.size() || lines.empty()) {
        lines.push_back({byteAt(start), text.size(),
                         spanWidth(chars, start, chars.size())});
      }
      return std::move(lines);
    }
  };

  /// "…", or three dots when the font has no ellipsis glyph.
  std::string_view ellipsisMark(const Setter& s) {
    if (s.pipeline == nullptr) {
      return ELLIPSIS;
    }
    const GlyphInfo* dot = glyphOf(s, ELLIPSIS_CODEPOINT);
    return dot != nullptr && dot->glyph_index != 0 ? ELLIPSIS : DOTS;
  }

  /// How many of @p chars fit @p room, trailing spaces dropped.
  std::size_t charsThatFit(const std::vector<SetChar>& chars, float room) {
    std::size_t keep = 0;
    while (keep < chars.size() && spanWidth(chars, 0, keep + 1) <= room) {
      ++keep;
    }
    while (keep > 0 && chars[keep - 1].codepoint == ' ') {
      --keep;
    }
    return keep;
  }

  /// @p v rounded to a whole device pixel of @p glyph's raster.
  float snapToPixel(float v, const GlyphInfo& glyph) {
    const float px =
        glyph.atlas_layout_scale > 0.0f ? glyph.atlas_layout_scale : 1.0f;
    return std::round(v / px) * px;
  }

  /// Draw one set character of @p draw, set by @p s, on the baseline
  /// @p baseline.
  void drawChar(const GuiDrawContext& ctx, const Setter& s, const SetChar& c,
                const LinePen& pen) {
    const float x = pen.draw.pos.x + c.x;
    if (c.glyph == nullptr || s.pipeline == nullptr) {
      const float w = placeholderAdvance(s.font);
      ctx.drawFilledRect({x, pen.draw.pos.y, w - 1.0f, w}, pen.draw.color);
      return;
    }
    // A tabular digit narrower than the widest sits centred in its slot.
    const float centre = s.tabular > 0.0f && isDigit(c.codepoint)
                             ? (s.tabular - c.glyph->advance) * 0.5f
                             : 0.0f;
    // Pens move in fractions; glyph bitmaps land on whole device pixels.
    ctx.renderer->emitGlyph({snapToPixel(x + centre, *c.glyph),
                             snapToPixel(pen.baseline, *c.glyph), *c.glyph,
                             1.0f, pen.draw.color.pack()});
  }

  /// Where @p draw's baseline is: an ascender below its top, plus half the
  /// leading a taller line height than the font's own adds.
  float baselineOf(const GuiDrawContext& ctx, const Setter& s,
                   const GuiTextDraw& draw) {
    const FontMetrics m = ctx.fontMetrics(s.font);
    const float natural =
        s.pipeline != nullptr
            ? s.pipeline->metrics(s.face, s.font.size).line_height
            : m.line_height;
    return draw.pos.y + m.ascender +
           std::max(0.0f, m.line_height - natural) * 0.5f;
  }


}  // namespace

uint32_t GuiDrawContext::faceFor(const GuiFont& font) const {
  if (text_pipeline == nullptr) {
    return face_id;
  }
  const auto italic = font.slant == GuiFontSlant::ITALIC
                          ? FontLoadItalic::ITALIC
                          : FontLoadItalic::NORMAL;
  return text_pipeline->faceFor(font.weight, italic).value_or(face_id);
}

FontMetrics GuiDrawContext::fontMetrics(const GuiFont& font) const {
  const Setter s = setterFor(*this, font);
  FontMetrics m = s.pipeline != nullptr
                      ? s.pipeline->metrics(s.face, font.size)
                      : FontMetrics{font.size, 0.0f, font.size};
  if (font.line_height > 0.0f) {
    m.line_height = font.size * font.line_height;
  }
  return m;
}

float GuiDrawContext::measureText(std::string_view text,
                                  const GuiFont& font) const {
  const std::vector<SetChar> chars = setText(setterFor(*this, font), text);
  return spanWidth(chars, 0, chars.size());
}

void GuiDrawContext::drawText(const GuiTextDraw& draw) const {
  if (renderer == nullptr || draw.text.empty()) {
    return;
  }
  const Setter s = setterFor(*this, draw.font);
  const LinePen pen{draw, baselineOf(*this, s, draw)};
  for (const SetChar& c : setText(s, draw.text)) {
    if (c.codepoint != ' ' && c.codepoint != '\n') {
      drawChar(*this, s, c, pen);
    }
  }
}

std::vector<GuiWrappedLine> GuiDrawContext::wrapText(std::string_view text,
                                                     const GuiFont& font,
                                                     float width) const {
  LineBreaker breaker{setText(setterFor(*this, font), text), text};
  for (std::size_t i = 0; i < breaker.chars.size(); ++i) {
    breaker.consider(i, width);
  }
  return breaker.finish();
}

std::string GuiDrawContext::ellipsize(std::string_view text,
                                      const GuiFont& font, float width) const {
  const Setter s = setterFor(*this, font);
  const std::vector<SetChar> chars = setText(s, text);
  if (spanWidth(chars, 0, chars.size()) <= width) {
    return std::string(text);
  }
  const std::string_view mark = ellipsisMark(s);
  const std::size_t keep = charsThatFit(chars, width - measureText(mark, font));
  const std::size_t cut = keep < chars.size() ? chars[keep].at : text.size();
  return std::string(text.substr(0, cut)).append(mark);
}

}  // namespace eng
