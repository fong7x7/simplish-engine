#include "engine/gui/gui-text-input.h"

#include "engine/gui/gui-draw-context.h"
#include "engine/gui/gui-style.h"
#include "engine/gui/gui-theme-constants.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

#include <algorithm>

namespace eng {
namespace {

  /// Horizontal text padding inside the input field.
  constexpr float INPUT_TEXT_PAD = 5.0f;
  /// Vertical text offset inside the input field.
  constexpr float INPUT_TEXT_VPAD = 3.0f;
  /// Width of the blinking cursor line in pixels.
  constexpr float CURSOR_WIDTH = 1.0f;
  /// Vertical inset from the input rect top/bottom for the cursor line.
  constexpr float CURSOR_VPAD = 3.0f;
  /// Default border width for text inputs.
  constexpr float INPUT_BORDER_WIDTH = 1.0f;
  /// Fallback selection highlight color when no shared style is set.
  constexpr GuiColor FALLBACK_SELECTION_BG{50, 100, 200};
  /// Fallback selected text color when no shared style is set.
  constexpr GuiColor FALLBACK_SELECTION_TEXT{255, 255, 255};
  /// Fallback placeholder text color when no shared style is set.
  constexpr GuiColor FALLBACK_PLACEHOLDER_COLOR{120, 120, 120};

  /// Whether the cursor should be visible at this point in the blink cycle.
  bool isCursorVisible(float timer) {
    return timer < CURSOR_BLINK_PERIOD / 2.0f;
  }

  /// True if the character is part of a word (not a delimiter).
  bool isWordChar(char c) {
    return c != ' ' && c != '\t' && c != '.' && c != ',' && c != ';' &&
           c != ':' && c != '!' && c != '?' && c != '(' && c != ')';
  }

}  // namespace

// ─── Lifecycle ───────────────────────────────────────────────

std::unique_ptr<GuiWidget> GuiTextInput::clone() const {
  return std::make_unique<GuiTextInput>(*this);
}

void GuiTextInput::update(const GuiDrawContext& ctx, float dt) {
  GuiWidget::update(ctx, dt);
  resolvePendingClick(ctx);
  resolveWordSelect(ctx);
  if (focus == GuiTextInputFocus::UNFOCUSED) {
    blink_timer_ = 0.0f;
    return;
  }
  blink_timer_ += dt;
  if (blink_timer_ >= CURSOR_BLINK_PERIOD) {
    blink_timer_ -= CURSOR_BLINK_PERIOD;
  }
}

// ─── Small helpers ───────────────────────────────────────────

void GuiTextInput::collapseSelection() {
  sel_anchor_ = cursor_pos_;
}

void GuiTextInput::notifyChange() {
  if (on_change) {
    on_change(buffer_);
  }
}

void GuiTextInput::resetBlink() {
  blink_timer_ = 0.0f;
  pending_click_x_.reset();
}

void GuiTextInput::eraseSelection() {
  auto lo = std::min(sel_anchor_, cursor_pos_);
  auto hi = std::max(sel_anchor_, cursor_pos_);
  buffer_.erase(lo, hi - lo);
  cursor_pos_ = lo;
  collapseSelection();
}

// ─── Accessors ───────────────────────────────────────────────

std::string_view GuiTextInput::text() const {
  return buffer_;
}

std::size_t GuiTextInput::cursorPos() const {
  return cursor_pos_;
}

bool GuiTextInput::hasSelection() const {
  return sel_anchor_ != cursor_pos_;
}

std::string_view GuiTextInput::selectedText() const {
  auto lo = std::min(sel_anchor_, cursor_pos_);
  auto hi = std::max(sel_anchor_, cursor_pos_);
  return std::string_view(buffer_).substr(lo, hi - lo);
}

// ─── Mutation methods ────────────────────────────────────────

void GuiTextInput::setText(std::string_view new_text) {
  buffer_ = new_text.substr(0, max_length);
  cursor_pos_ = buffer_.size();
  collapseSelection();
  resetBlink();
  notifyChange();
}

void GuiTextInput::insertAtCursor(std::string_view input) {
  if (hasSelection()) {
    eraseSelection();
  }
  auto remaining = max_length - buffer_.size();
  if (remaining == 0) {
    return;
  }
  auto to_insert = input.substr(0, remaining);
  buffer_.insert(cursor_pos_, to_insert);
  cursor_pos_ += to_insert.size();
  collapseSelection();
  resetBlink();
  notifyChange();
}

void GuiTextInput::appendText(std::string_view input) {
  cursor_pos_ = buffer_.size();
  collapseSelection();
  insertAtCursor(input);
}

void GuiTextInput::deleteBack() {
  if (hasSelection()) {
    eraseSelection();
    resetBlink();
    notifyChange();
    return;
  }
  if (cursor_pos_ == 0) {
    return;
  }
  buffer_.erase(cursor_pos_ - 1, 1);
  --cursor_pos_;
  collapseSelection();
  resetBlink();
  notifyChange();
}

void GuiTextInput::deleteForward() {
  if (hasSelection()) {
    eraseSelection();
    resetBlink();
    notifyChange();
    return;
  }
  if (cursor_pos_ >= buffer_.size()) {
    return;
  }
  buffer_.erase(cursor_pos_, 1);
  collapseSelection();
  resetBlink();
  notifyChange();
}

void GuiTextInput::selectAll() {
  sel_anchor_ = 0;
  cursor_pos_ = buffer_.size();
  resetBlink();
}

void GuiTextInput::applyCursorMove(std::size_t pos, GuiSelectionExtend mode) {
  cursor_pos_ = pos;
  if (mode == GuiSelectionExtend::COLLAPSE) {
    collapseSelection();
  }
  resetBlink();
}

void GuiTextInput::moveLeft(GuiSelectionExtend mode) {
  if (mode == GuiSelectionExtend::COLLAPSE && hasSelection()) {
    applyCursorMove(std::min(sel_anchor_, cursor_pos_), mode);
  } else if (cursor_pos_ > 0) {
    applyCursorMove(cursor_pos_ - 1, mode);
  }
}

void GuiTextInput::moveRight(GuiSelectionExtend mode) {
  if (mode == GuiSelectionExtend::COLLAPSE && hasSelection()) {
    applyCursorMove(std::max(sel_anchor_, cursor_pos_), mode);
  } else if (cursor_pos_ < buffer_.size()) {
    applyCursorMove(cursor_pos_ + 1, mode);
  }
}

void GuiTextInput::dispatchCursorKey(uint32_t keycode,
                                     GuiSelectionExtend mode) {
  switch (keycode) {
    case SDLK_LEFT:
      moveLeft(mode);
      break;
    case SDLK_RIGHT:
      moveRight(mode);
      break;
    case SDLK_HOME:
      applyCursorMove(0, mode);
      break;
    case SDLK_END:
      applyCursorMove(buffer_.size(), mode);
      break;
    default:
      break;
  }
}

void GuiTextInput::moveCursor(const GuiKeyEvent& event) {
  auto mode =
      event.shift ? GuiSelectionExtend::EXTEND : GuiSelectionExtend::COLLAPSE;
  dispatchCursorKey(event.keycode, mode);
}

// ─── Click-to-cursor ─────────────────────────────────────────

void GuiTextInput::setCursorClickX(float mx, GuiSelectionExtend mode) {
  pending_click_x_ = mx;
  pending_click_mode_ = mode;
}

float GuiTextInput::measurePrefix(const GuiDrawContext& ctx,
                                  std::size_t len) const {
  if (len == 0) {
    return 0.0f;
  }
  std::string_view prefix{buffer_.data(), len};
  return ctx.measureText(prefix);
}

std::size_t GuiTextInput::hitTestCursorPos(const GuiDrawContext& ctx,
                                           float local_x) const {
  float best_dist = std::abs(local_x);
  std::size_t best_pos = 0;
  for (std::size_t i = 1; i <= buffer_.size(); ++i) {
    float dist = std::abs(local_x - measurePrefix(ctx, i));
    if (dist < best_dist) {
      best_dist = dist;
      best_pos = i;
    }
  }
  return best_pos;
}

void GuiTextInput::resolvePendingClick(const GuiDrawContext& ctx) {
  if (!pending_click_x_.has_value()) {
    return;
  }
  float local_x = *pending_click_x_ - rect.x - INPUT_TEXT_PAD;
  pending_click_x_.reset();
  cursor_pos_ = hitTestCursorPos(ctx, local_x);
  if (pending_click_mode_ == GuiSelectionExtend::COLLAPSE) {
    sel_anchor_ = cursor_pos_;
  }
  pending_click_mode_ = GuiSelectionExtend::COLLAPSE;
  blink_timer_ = 0.0f;
}

void GuiTextInput::setPendingWordSelect(float mx) {
  pending_word_select_x_ = mx;
}

std::size_t GuiTextInput::wordStart(const std::string& buf, std::size_t pos) {
  if (pos == 0) {
    return 0;
  }
  bool in_word = pos < buf.size() && isWordChar(buf[pos]);
  std::size_t i = pos;
  while (i > 0 && isWordChar(buf[i - 1]) == in_word) {
    --i;
  }
  return i;
}

std::size_t GuiTextInput::wordEnd(const std::string& buf, std::size_t pos) {
  bool in_word = pos < buf.size() && isWordChar(buf[pos]);
  std::size_t i = pos;
  while (i < buf.size() && isWordChar(buf[i]) == in_word) {
    ++i;
  }
  return i;
}

void GuiTextInput::resolveWordSelect(const GuiDrawContext& ctx) {
  if (!pending_word_select_x_.has_value()) {
    return;
  }
  float local_x = *pending_word_select_x_ - rect.x - INPUT_TEXT_PAD;
  pending_word_select_x_.reset();
  auto pos = hitTestCursorPos(ctx, local_x);
  if (pos == buffer_.size() && pos > 0) {
    --pos;
  }
  sel_anchor_ = wordStart(buffer_, pos);
  cursor_pos_ = wordEnd(buffer_, pos);
  blink_timer_ = 0.0f;
}

// ─── Rendering ───────────────────────────────────────────────

void GuiTextInput::renderBox(const GuiDrawContext& ctx) const {
  const bool use_shared = hasSharedStyle();
  auto bg = GuiColor::applyOpacity(use_shared ? ui_style->input_bg : fill_color,
                                   opacity);
  float rad = use_shared ? ui_style->input_corner_radius : corner_radius;
  ctx.drawRoundedRect(rect, bg, rad);
  auto bc = use_shared ? ui_style->input_border : border_color;
  if (focus == GuiTextInputFocus::FOCUSED && use_shared) {
    bc = ui_style->input_border_focused;
  }
  bc = GuiColor::applyOpacity(bc, opacity);
  float bw = use_shared ? INPUT_BORDER_WIDTH : border_width;
  if (bw > 0.0f) {
    ctx.drawRoundedBorderRect({rect, bc, rad, bw});
  }
}

GuiColor GuiTextInput::selectionBgColor() const {
  return hasSharedStyle() ? ui_style->input_selection_bg
                          : FALLBACK_SELECTION_BG;
}

GuiColor GuiTextInput::selectionTextColor() const {
  return hasSharedStyle() ? ui_style->input_selection_text
                          : FALLBACK_SELECTION_TEXT;
}

void GuiTextInput::renderSelectionHighlight(const GuiDrawContext& ctx,
                                            float text_x) const {
  if (!hasSelection()) {
    return;
  }
  auto lo = std::min(sel_anchor_, cursor_pos_);
  auto hi = std::max(sel_anchor_, cursor_pos_);
  float x0 = text_x + measurePrefix(ctx, lo);
  float x1 = text_x + measurePrefix(ctx, hi);
  float cy = rect.y + CURSOR_VPAD;
  float ch = rect.h - 2.0f * CURSOR_VPAD;
  ctx.drawFilledRect({x0, cy, x1 - x0, ch},
                     GuiColor::applyOpacity(selectionBgColor(), opacity));
  std::string_view sel_str{buffer_.data() + lo, hi - lo};
  ctx.drawText(GuiColor::applyOpacity(selectionTextColor(), opacity),
               {x0, rect.y + INPUT_TEXT_VPAD}, sel_str);
}

void GuiTextInput::renderCursorLine(const GuiDrawContext& ctx,
                                    float text_x) const {
  if (focus != GuiTextInputFocus::FOCUSED) {
    return;
  }
  if (!isCursorVisible(blink_timer_)) {
    return;
  }
  const bool use_shared = hasSharedStyle();
  auto tc = GuiColor::applyOpacity(
      use_shared ? ui_style->input_text : text_color, opacity);
  float cx = text_x + measurePrefix(ctx, cursor_pos_);
  float cy = rect.y + CURSOR_VPAD;
  float ch = rect.h - 2.0f * CURSOR_VPAD;
  ctx.drawFilledRect({cx, cy, CURSOR_WIDTH, ch}, tc);
}

void GuiTextInput::renderPlaceholder(const GuiDrawContext& ctx,
                                     float text_x) const {
  if (!buffer_.empty() || placeholder.empty()) {
    return;
  }
  const bool use_shared = hasSharedStyle();
  auto tc = GuiColor::applyOpacity(
      use_shared ? ui_style->text_dim : FALLBACK_PLACEHOLDER_COLOR, opacity);
  ctx.drawText(tc, {text_x, rect.y + INPUT_TEXT_VPAD}, placeholder);
}

void GuiTextInput::renderTextAndCursor(const GuiDrawContext& ctx) const {
  float text_x = rect.x + INPUT_TEXT_PAD;
  if (!buffer_.empty()) {
    const bool use_shared = hasSharedStyle();
    auto tc = GuiColor::applyOpacity(
        use_shared ? ui_style->input_text : text_color, opacity);
    ctx.drawText(tc, {text_x, rect.y + INPUT_TEXT_VPAD}, buffer_);
  }
  renderPlaceholder(ctx, text_x);
  renderSelectionHighlight(ctx, text_x);
  renderCursorLine(ctx, text_x);
}

void GuiTextInput::render(const GuiDrawContext& ctx) const {
  renderBox(ctx);
  renderTextAndCursor(ctx);
}

}  // namespace eng
