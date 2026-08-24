#include "engine/gui/gui-text-area.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

#include <algorithm>

namespace eng {
namespace {

  /// Width of the blinking cursor line in pixels.
  constexpr float AREA_CURSOR_WIDTH = 1.0f;
  /// Vertical inset for cursor line from each line's top/bottom.
  constexpr float AREA_CURSOR_VPAD = 1.0f;
  /// Fallback placeholder text color.
  constexpr GuiColor AREA_PLACEHOLDER_COLOR{120, 120, 120};

}  // namespace

// ─── Clone ──────────────────────────────────────────────────

// NOLINTNEXTLINE(misc-use-internal-linkage) -- virtual override
std::unique_ptr<GuiWidget> GuiTextArea::clone() const {
  return std::make_unique<GuiTextArea>(*this);
}

// ─── Line wrapping ──────────────────────────────────────────

// Named algorithm: word-wrap the buffer into display lines that fit within
// the widget's available width.  Splits on newlines first, then wraps long
// segments at word boundaries using measureText.  No side effects.
std::vector<GuiTextArea::WrappedLine>
GuiTextArea::splitLines(const GuiDrawContext& ctx) const {
  std::vector<WrappedLine> lines;
  float max_w = rect.w - 2.0f * text_padding;
  if (max_w <= 0.0f) {
    return lines;
  }
  std::size_t pos = 0;
  while (pos <= buffer_.size()) {
    auto nl = buffer_.find('\n', pos);
    if (nl == std::string::npos) {
      nl = buffer_.size();
    }
    auto segment = buffer_.substr(pos, nl - pos);
    if (segment.empty()) {
      lines.push_back({pos, 0});
    } else {
      wrapSegment(ctx, {&lines, pos, segment, max_w});
    }
    pos = nl + 1;
    if (nl == buffer_.size()) {
      break;
    }
  }
  if (lines.empty()) {
    lines.push_back({0, 0});
  }
  return lines;
}

// Named algorithm: wrap a single newline-free segment into one or more
// display lines by measuring words.  Greedily fits as many words as
// possible per line.  No side effects beyond appending to params.out.
void GuiTextArea::wrapSegment(const GuiDrawContext& ctx,
                              const WrapSegmentParams& params) const {
  std::size_t line_start = 0;
  std::size_t i = 0;
  while (i < params.seg.size()) {
    auto end = i;
    while (end < params.seg.size() && params.seg[end] != ' ') {
      ++end;
    }
    auto test = params.seg.substr(line_start, end - line_start);
    float tw = ctx.measureText(test);
    if (tw > params.max_w && i > line_start) {
      params.out->push_back({params.seg_start + line_start, i - line_start});
      line_start = i;
    }
    i = end;
    if (i < params.seg.size()) {
      ++i;
    }
  }
  params.out->push_back(
      {params.seg_start + line_start, params.seg.size() - line_start});
}

// ─── Line/column helpers ────────────────────────────────────

std::size_t GuiTextArea::findLineForPos(const std::vector<WrappedLine>& lines,
                                        std::size_t pos) {
  for (std::size_t i = 0; i < lines.size(); ++i) {
    auto end = lines[i].start + lines[i].length;
    if (pos <= end) {
      return i;
    }
  }
  return lines.empty() ? 0 : lines.size() - 1;
}

std::size_t GuiTextArea::columnInLine(const WrappedLine& line,
                                      std::size_t pos) {
  if (pos < line.start) {
    return 0;
  }
  return std::min(pos - line.start, line.length);
}

// ─── Cursor movement ────────────────────────────────────────

// NOLINTNEXTLINE(misc-use-internal-linkage) -- virtual override
void GuiTextArea::moveCursor(const GuiKeyEvent& event) {
  if (event.keycode == SDLK_UP || event.keycode == SDLK_DOWN) {
    auto mode =
        event.shift ? GuiSelectionExtend::EXTEND : GuiSelectionExtend::COLLAPSE;
    // NOLINTBEGIN(bugprone-branch-clone) — moveUp and moveDown are distinct
    // functions
    if (event.keycode == SDLK_UP) {
      moveUp(mode, cached_lines_);
    } else {
      moveDown(mode, cached_lines_);
    }
    // NOLINTEND(bugprone-branch-clone)
    return;
  }
  GuiTextInput::moveCursor(event);
}

void GuiTextArea::moveUp(GuiSelectionExtend mode,
                         const std::vector<WrappedLine>& lines) {
  if (lines.empty()) {
    return;
  }
  auto li = findLineForPos(lines, cursor_pos_);
  if (li == 0) {
    applyCursorMove(0, mode);
    return;
  }
  auto col = columnInLine(lines[li], cursor_pos_);
  const auto& prev = lines[li - 1];
  auto new_pos = prev.start + std::min(col, prev.length);
  applyCursorMove(new_pos, mode);
}

void GuiTextArea::moveDown(GuiSelectionExtend mode,
                           const std::vector<WrappedLine>& lines) {
  if (lines.empty()) {
    return;
  }
  auto li = findLineForPos(lines, cursor_pos_);
  if (li >= lines.size() - 1) {
    applyCursorMove(buffer_.size(), mode);
    return;
  }
  auto col = columnInLine(lines[li], cursor_pos_);
  const auto& next = lines[li + 1];
  auto new_pos = next.start + std::min(col, next.length);
  applyCursorMove(new_pos, mode);
}

// NOLINTNEXTLINE(misc-use-internal-linkage) -- virtual override
void GuiTextArea::insertAtCursor(std::string_view input) {
  GuiTextInput::insertAtCursor(input);
}

// ─── Rendering ──────────────────────────────────────────────

GuiColor GuiTextArea::resolveTextColor() const {
  const bool use_shared = hasSharedStyle();
  return GuiColor::applyOpacity(use_shared ? ui_style->input_text : text_color,
                                opacity);
}

void GuiTextArea::renderLines(const GuiDrawContext& ctx,
                              const std::vector<WrappedLine>& lines) const {
  if (buffer_.empty()) {
    return;
  }
  auto tc = resolveTextColor();
  float lh = ctx.textLineHeight();
  float x = rect.x + text_padding;
  float y = rect.y + text_vpadding;
  for (const auto& line : lines) {
    if (y > rect.y + rect.h) {
      break;
    }
    std::string_view str{buffer_.data() + line.start, line.length};
    ctx.drawText(tc, {x, y}, str);
    y += lh;
  }
}

void GuiTextArea::renderAreaCursor(
    const GuiDrawContext& ctx, const std::vector<WrappedLine>& lines) const {
  if (focus != GuiTextInputFocus::FOCUSED) {
    return;
  }
  if (blink_timer_ >= CURSOR_BLINK_PERIOD / 2.0f) {
    return;
  }
  float lh = ctx.textLineHeight();
  auto li = findLineForPos(lines, cursor_pos_);
  auto col = columnInLine(lines[li], cursor_pos_);
  std::string_view prefix{buffer_.data() + lines[li].start, col};
  float cx = rect.x + text_padding + ctx.measureText(prefix);
  float cy = rect.y + text_vpadding + static_cast<float>(li) * lh;
  ctx.drawFilledRect({cx, cy + AREA_CURSOR_VPAD, AREA_CURSOR_WIDTH,
                      lh - 2.0f * AREA_CURSOR_VPAD},
                     resolveTextColor());
}

void GuiTextArea::renderAreaSelection(
    const GuiDrawContext& ctx, const std::vector<WrappedLine>& lines) const {
  if (!hasSelection()) {
    return;
  }
  auto lo = std::min(sel_anchor_, cursor_pos_);
  auto hi = std::max(sel_anchor_, cursor_pos_);
  float lh = ctx.textLineHeight();
  float x0 = rect.x + text_padding;
  auto sel_bg = GuiColor::applyOpacity(selectionBgColor(), opacity);
  for (std::size_t i = 0; i < lines.size(); ++i) {
    LineSelectionParams lsp{&lines[i], i, lo, hi, lh, x0, sel_bg};
    renderSelectionLine(ctx, lsp);
  }
}

void GuiTextArea::renderSelectionLine(const GuiDrawContext& ctx,
                                      const LineSelectionParams& params) const {
  auto line_end = params.line->start + params.line->length;
  if (params.sel_hi <= params.line->start || params.sel_lo >= line_end) {
    return;
  }
  auto s = std::max(params.sel_lo, params.line->start) - params.line->start;
  auto e = std::min(params.sel_hi, line_end) - params.line->start;
  std::string_view pre_s{buffer_.data() + params.line->start, s};
  std::string_view pre_e{buffer_.data() + params.line->start, e};
  float sx = params.text_x + ctx.measureText(pre_s);
  float ex = params.text_x + ctx.measureText(pre_e);
  float sy = rect.y + text_vpadding +
             static_cast<float>(params.line_idx) * params.line_height;
  ctx.drawFilledRect({sx, sy, ex - sx, params.line_height}, params.sel_bg);
}

void GuiTextArea::renderAreaPlaceholder(const GuiDrawContext& ctx) const {
  if (!buffer_.empty() || placeholder.empty()) {
    return;
  }
  const bool use_shared = hasSharedStyle();
  auto tc = GuiColor::applyOpacity(
      use_shared ? ui_style->text_dim : AREA_PLACEHOLDER_COLOR, opacity);
  ctx.drawText(tc, {rect.x + text_padding, rect.y + text_vpadding},
               placeholder);
}

// NOLINTNEXTLINE(misc-use-internal-linkage) -- virtual override
void GuiTextArea::render(const GuiDrawContext& ctx) const {
  renderBox(ctx);
  cached_lines_ = splitLines(ctx);
  renderAreaSelection(ctx, cached_lines_);
  renderLines(ctx, cached_lines_);
  renderAreaPlaceholder(ctx);
  renderAreaCursor(ctx, cached_lines_);
}

}  // namespace eng
