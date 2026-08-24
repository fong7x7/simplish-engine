#include <engine/gui/shaped-run.h>

namespace eng {
namespace {

  TextLine makeLine(uint32_t start, uint32_t end, float width) {
    TextLine line;
    line.start_index = start;
    line.end_index = end;
    line.width = width;
    return line;
  }

  void emitLine(std::vector<TextLine>& lines, uint32_t& line_start,
                float& line_width, uint32_t end_index) {
    lines.push_back(makeLine(line_start, end_index, line_width));
    line_start = end_index;
    line_width = 0.0f;
  }

}  // namespace

std::vector<TextLine> ShapedRun::breakLines(float max_width) const {
  if (glyphs.empty()) {
    return {};
  }

  std::vector<TextLine> lines;
  uint32_t line_start = 0;
  float line_width = 0.0f;

  for (uint32_t i = 0; i < glyphs.size(); ++i) {
    float glyph_w = glyphs[i].x_advance;
    if (line_width + glyph_w > max_width && line_start != i) {
      emitLine(lines, line_start, line_width, i);
    }
    line_width += glyph_w;
  }
  lines.push_back(
      makeLine(line_start, static_cast<uint32_t>(glyphs.size()), line_width));
  return lines;
}

}  // namespace eng
