#pragma once

#include "gui-color.h"
#include "gui-font.h"
#include "gui-text-draw.h"
#include "gui-text-overflow.h"
#include "gui-text-role.h"
#include "gui-text-wrap.h"
#include "gui-widget.h"
#include "gui-wrapped-line.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace eng {

/// Text alignment mode for labels.
enum class GuiLabelAlign : uint8_t {
  /// Left-aligned at rect.x, rect.y.
  LEFT,
  /// Centered both horizontally and vertically within rect.
  CENTER,
  /// Centered horizontally at rect center X, drawn at rect.y.
  H_CENTER,
  /// Right-aligned at the rect's right edge, drawn from rect.y.
  RIGHT,
};

/// A text label: one line or wrapped to its width, set in its theme role's
/// font (or its own), aligned, and cut short with "…" if asked.
/// Non-interactive by default.
/// @thread_safety Main thread only.
class GuiLabel : public GuiWidget {
public:
  /// Polymorphic deep-copy.
  std::unique_ptr<GuiWidget> clone() const override;

  /// Render this label.
  void render(const GuiDrawContext& ctx) const override;

  /// The text's width and height: one line per newline, or wrapped to
  /// @p max_width when `wrap` is WORD.
  [[nodiscard]] LayoutSize measureContent(const GuiDrawContext& ctx,
                                          float max_width) const override;

  /// Text string to render.
  std::string_view text{};
  /// Text colour; unset takes the theme's `palette.text`.
  std::optional<GuiColor> color{};
  /// Alignment mode.
  GuiLabelAlign align = GuiLabelAlign::LEFT;
  /// What it is, which picks its font from the theme.
  GuiTextRole role = GuiTextRole::BODY;
  /// Its own font, in place of its role's.
  std::optional<GuiFont> font{};
  /// Whether it wraps to its width.
  GuiTextWrap wrap = GuiTextWrap::NONE;
  /// Whether a line too long for its width ends in "…".
  GuiTextOverflow overflow = GuiTextOverflow::VISIBLE;

  /// The font it is set in: `font`, else its role's in @p ctx's theme.
  [[nodiscard]] GuiFont resolvedFont(const GuiDrawContext& ctx) const;

private:
  /// Draw one of its lines as @p style says — at its y, colour and font —
  /// aligned across its rect and cut short with "…" if asked.
  void drawLine(const GuiDrawContext& ctx, const GuiWrappedLine& line,
                const GuiTextDraw& style) const;
};

}  // namespace eng
