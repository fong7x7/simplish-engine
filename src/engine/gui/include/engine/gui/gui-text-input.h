#pragma once

/// @file gui-text-input.h
/// @brief Single-line field: focus, UTF-8 buffer, cursor, selection, SDL
/// text/key via manager.
/// Design: docs/technical-approaches/editor/ui-text-input-interaction.md
/// @par Threading Main thread only.

#include "gui-color.h"
#include "gui-draw-context.h"
#include "gui-key-event.h"
#include "gui-panel.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace eng {

/// Whether a cursor move should extend the current selection.
enum class GuiSelectionExtend : uint8_t {
  /// Collapse selection to cursor position.
  COLLAPSE,
  /// Keep anchor, extend selection to new cursor position.
  EXTEND,
};

/// Focus state for a text input (enum class, not bool per coding standards).
enum class GuiTextInputFocus : uint8_t {
  /// Input is not focused.
  UNFOCUSED,
  /// Input has keyboard focus.
  FOCUSED,
};

/// Maximum character count for text input fields.
inline constexpr std::size_t DEFAULT_TEXT_INPUT_MAX_LENGTH = 256;

/// Full blink cycle period in seconds (on + off).
inline constexpr float CURSOR_BLINK_PERIOD = 1.0f;

/// Extends GuiPanel for background fill, rounded corners, and borders.
/// @thread_safety Main thread only.
class GuiTextInput : public GuiPanel {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Tick the cursor blink timer and resolve any pending click.
  void update(const GuiDrawContext& ctx, float dt) override;

  /// Render this text input field.
  void render(const GuiDrawContext& ctx) const override;

  /// Get the current text content.
  [[nodiscard]] std::string_view text() const;

  /// Set the text content programmatically. Fires on_change.
  void setText(std::string_view new_text);

  /// Append text at end of buffer. Respects max_length. Fires on_change.
  void appendText(std::string_view input);

  /// Insert text at cursor position, replacing selection. Fires on_change.
  virtual void insertAtCursor(std::string_view input);

  /// Delete the character before the cursor, or selection. Fires on_change.
  void deleteBack();

  /// Delete the character after the cursor, or selection. Fires on_change.
  void deleteForward();

  /// Move cursor via Left/Right/Home/End, with shift-selection.
  virtual void moveCursor(const GuiKeyEvent& event);

  /// Select all text (anchor=0, cursor=end).
  void selectAll();

  /// True if anchor and cursor differ (text is selected).
  [[nodiscard]] bool hasSelection() const;

  /// Return the selected substring (empty if no selection).
  [[nodiscard]] std::string_view selectedText() const;

  /// Current cursor byte position (0 to buffer size).
  [[nodiscard]] std::size_t cursorPos() const;

  /// Store a pending click X for deferred cursor placement.
  void setCursorClickX(float mx, GuiSelectionExtend mode);

  /// Store a pending double-click X for deferred word selection.
  void setPendingWordSelect(float mx);

  /// Text color.
  GuiColor text_color{};
  /// Whether this input has keyboard focus.
  GuiTextInputFocus focus = GuiTextInputFocus::UNFOCUSED;
  /// Callback fired when text content changes.
  std::function<void(std::string_view)> on_change{};
  /// Maximum number of characters allowed.
  std::size_t max_length = DEFAULT_TEXT_INPUT_MAX_LENGTH;
  /// Placeholder text shown when the buffer is empty (not owned).
  std::string_view placeholder{};

protected:
  /// Render the background box and border.
  void renderBox(const GuiDrawContext& ctx) const;

  /// Erase the selected range and collapse cursor to range start.
  void eraseSelection();

  /// Set selection anchor equal to cursor position (no selection).
  void collapseSelection();

  /// Fire on_change callback if registered.
  void notifyChange();

  /// Reset the blink timer to make cursor immediately visible.
  void resetBlink();

  /// Move cursor to @p pos and extend or collapse selection.
  void applyCursorMove(std::size_t pos, GuiSelectionExtend mode);

  /// Measure the pixel width of the first @p len bytes of buffer_.
  [[nodiscard]] float measurePrefix(const GuiDrawContext& ctx,
                                    std::size_t len) const;

  /// Resolve selection background color from shared style or fallback.
  [[nodiscard]] GuiColor selectionBgColor() const;

  /// Resolve selected text color from shared style or fallback.
  [[nodiscard]] GuiColor selectionTextColor() const;

  /// Owned text buffer.
  std::string buffer_{};
  /// Accumulated blink timer in seconds (wraps at CURSOR_BLINK_PERIOD).
  float blink_timer_ = 0.0f;
  /// Cursor byte position within buffer_ (0 to buffer_.size()).
  std::size_t cursor_pos_ = 0;
  /// Selection anchor position; equals cursor_pos_ when no selection.
  std::size_t sel_anchor_ = 0;

private:
  /// Render text, selection highlight, and blinking cursor.
  void renderTextAndCursor(const GuiDrawContext& ctx) const;

  /// Move cursor one position left, collapsing selection if needed.
  void moveLeft(GuiSelectionExtend mode);

  /// Move cursor one position right, collapsing selection if needed.
  void moveRight(GuiSelectionExtend mode);

  /// Dispatch a cursor-movement keycode to the appropriate handler.
  void dispatchCursorKey(uint32_t keycode, GuiSelectionExtend mode);

  /// Find the cursor byte position closest to a local X offset.
  [[nodiscard]] std::size_t hitTestCursorPos(const GuiDrawContext& ctx,
                                             float local_x) const;

  /// Convert pending_click_x_ to a cursor position using font metrics.
  void resolvePendingClick(const GuiDrawContext& ctx);

  /// Resolve a pending double-click word selection.
  void resolveWordSelect(const GuiDrawContext& ctx);

  /// Find the start of the word containing byte position @p pos.
  [[nodiscard]] static std::size_t wordStart(const std::string& buf,
                                             std::size_t pos);

  /// Find the end of the word containing byte position @p pos.
  [[nodiscard]] static std::size_t wordEnd(const std::string& buf,
                                           std::size_t pos);

  /// Draw the selection highlight rectangle behind selected text.
  void renderSelectionHighlight(const GuiDrawContext& ctx, float text_x) const;

  /// Draw placeholder text when the buffer is empty.
  void renderPlaceholder(const GuiDrawContext& ctx, float text_x) const;

  /// Draw the blinking cursor line at cursor_pos_.
  void renderCursorLine(const GuiDrawContext& ctx, float text_x) const;

  /// Deferred click X coordinate for cursor placement (resolved in update).
  std::optional<float> pending_click_x_{};
  /// Selection mode for the pending click.
  GuiSelectionExtend pending_click_mode_ = GuiSelectionExtend::COLLAPSE;
  /// Deferred double-click X for word selection (resolved in update).
  std::optional<float> pending_word_select_x_{};
};

}  // namespace eng
