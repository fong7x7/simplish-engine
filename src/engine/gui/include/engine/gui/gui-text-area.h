#pragma once

/// @file gui-text-area.h
/// @brief Multi-line text area with word wrapping, extending GuiTextInput.
/// Supports newline insertion via Enter, vertical cursor movement via
/// Up/Down arrows, and multi-line selection highlighting.
/// @par Threading Main thread only.

#include "gui-text-input.h"

#include <cstddef>
#include <string>
#include <vector>

namespace eng {

/// Default maximum character count for text area fields.
inline constexpr std::size_t DEFAULT_TEXT_AREA_MAX_LENGTH = 4096;

/// A multi-line text area extending GuiTextInput with word wrapping.
/// Overrides rendering and cursor movement for multi-line editing.
/// @thread_safety Main thread only.
class GuiTextArea : public GuiTextInput {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this text area with wrapped lines.
  void render(const GuiDrawContext& ctx) const override;

  /// Horizontal text padding inside the area in pixels.
  float text_padding = 5.0f;
  /// Vertical text padding inside the area in pixels.
  float text_vpadding = 3.0f;

  /// Insert text at cursor, including newlines. Fires on_change.
  void insertAtCursor(std::string_view input) override;

  /// Move cursor via arrow keys, including Up/Down for vertical nav.
  void moveCursor(const GuiKeyEvent& event) override;

private:
  /// A single display line produced by word wrapping.
  struct WrappedLine {
    /// Byte offset of this line's start in the buffer.
    std::size_t start = 0;
    /// Number of bytes in this line (excluding any trailing newline).
    std::size_t length = 0;
  };

  /// Split buffer into word-wrapped lines for the current rect width.
  [[nodiscard]] std::vector<WrappedLine>
  splitLines(const GuiDrawContext& ctx) const;

  /// Parameters for wrapping a single newline-free segment.
  struct WrapSegmentParams {
    /// Output vector to append wrapped lines to.
    std::vector<WrappedLine>* out = nullptr;
    /// Byte offset of the segment start in the buffer.
    std::size_t seg_start = 0;
    /// The segment text to wrap.
    std::string_view seg{};
    /// Maximum line width in pixels.
    float max_w = 0.0f;
  };

  /// Wrap a single newline-free segment into display lines.
  void wrapSegment(const GuiDrawContext& ctx,
                   const WrapSegmentParams& params) const;

  /// Find which wrapped line contains the given byte position.
  [[nodiscard]] static std::size_t
  findLineForPos(const std::vector<WrappedLine>& lines, std::size_t pos);

  /// Compute the column offset within a line for a byte position.
  [[nodiscard]] static std::size_t columnInLine(const WrappedLine& line,
                                                std::size_t pos);

  /// Resolve the text color with opacity for this area.
  [[nodiscard]] GuiColor resolveTextColor() const;

  /// Render all wrapped lines of text.
  void renderLines(const GuiDrawContext& ctx,
                   const std::vector<WrappedLine>& lines) const;

  /// Render the blinking cursor at the correct line and column.
  void renderAreaCursor(const GuiDrawContext& ctx,
                        const std::vector<WrappedLine>& lines) const;

  /// Render selection highlight across multiple lines.
  void renderAreaSelection(const GuiDrawContext& ctx,
                           const std::vector<WrappedLine>& lines) const;

  /// Parameters for rendering selection on a single wrapped line.
  struct LineSelectionParams {
    /// The wrapped line to render selection for.
    const WrappedLine* line = nullptr;
    /// Index of this line in the wrapped lines array.
    std::size_t line_idx = 0;
    /// Start byte of the selection range.
    std::size_t sel_lo = 0;
    /// End byte of the selection range.
    std::size_t sel_hi = 0;
    /// Line height in pixels.
    float line_height = 0.0f;
    /// X origin for text rendering.
    float text_x = 0.0f;
    /// Selection background color.
    GuiColor sel_bg{};
  };

  /// Render selection highlight for a single wrapped line.
  void renderSelectionLine(const GuiDrawContext& ctx,
                           const LineSelectionParams& params) const;

  /// Render placeholder text when the buffer is empty.
  void renderAreaPlaceholder(const GuiDrawContext& ctx) const;

  /// Move cursor one line up, preserving approximate column.
  void moveUp(GuiSelectionExtend mode, const std::vector<WrappedLine>& lines);

  /// Move cursor one line down, preserving approximate column.
  void moveDown(GuiSelectionExtend mode, const std::vector<WrappedLine>& lines);

  /// Cached lines from the last render pass (mutable for const render).
  mutable std::vector<WrappedLine> cached_lines_;
};

}  // namespace eng
