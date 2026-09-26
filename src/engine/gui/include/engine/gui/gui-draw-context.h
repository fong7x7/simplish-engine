#pragma once

/// @file gui-draw-context.h
/// @brief Immutable handles for drawing text and rects through the GUI RHI
/// path.
/// @par Threading Main thread only.

#include "draw-pos.h"
#include "font-metrics.h"
#include "gui-color.h"
#include "gui-corners.h"
#include "gui-font.h"
#include "gui-nine-slice.h"
#include "gui-rect-paint.h"
#include "gui-renderer.h"
#include "gui-shadow.h"
#include "gui-state-style.h"
#include "gui-text-draw.h"
#include "gui-theme.h"
#include "gui-wrapped-line.h"
#include "text-pipeline.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace eng {

/// System cursor shape for SDL cursor updates.
enum class GuiCursorShape : uint8_t {
  /// Default arrow cursor.
  ARROW,
  /// Hand pointer cursor (for clickable elements).
  POINTER,
  /// I-beam cursor (for text input fields).
  IBEAM,
};

/// Set the system cursor to the specified shape (cached SDL cursors).
/// @thread_safety Main thread only.
void setUiCursor(GuiCursorShape shape);

/// Renderer + text pipeline handles; `const` methods may still mutate atlas
/// state via `*text_pipeline`.
/// @thread_safety Main thread only.
class GuiDrawContext {
public:
  /// GUI renderer context for quad/glyph emission (not owned).
  GuiRendererContext* renderer = nullptr;
  /// Text pipeline for glyph rasterization (null if no font, not owned).
  TextPipelineContext* text_pipeline = nullptr;
  /// Active font face ID for text rendering.
  uint32_t face_id = 0;
  /// The theme widgets draw from (not owned); null for `GuiTheme::dark()`.
  const GuiTheme* theme = nullptr;

  /// The theme to draw from: `theme`, or the dark preset without one.
  [[nodiscard]] const GuiTheme& activeTheme() const;

  /// Draw a solid-filled rectangle.
  void drawFilledRect(const Rect& rect, const GuiColor& color) const;

  /// Draw a filled rectangle with rounded corners.
  void drawRoundedRect(const Rect& rect, const GuiColor& color,
                       float radius) const;

  /// Draw @p style's box over @p rect — the theme's shadow for its
  /// elevation, its fill, then its border, at its radius — with every
  /// colour's alpha scaled by @p opacity. What a themed widget draws its
  /// background with.
  void drawBox(const Rect& rect, const GuiStateStyle& style,
               float opacity) const;

  /// Paint @p paint: its fill or gradient, then its border, at its corner
  /// radii — any box CSS could draw with `background`, `border` and
  /// `border-radius`.
  void drawRect(const GuiRectPaint& paint) const;

  /// Draw @p shadow under a box at @p rect with @p radii: offset, grown by
  /// its spread and softened by its blur, as CSS's `box-shadow`. Draw it
  /// before the box.
  void drawShadow(const Rect& rect, const GuiCorners& radii,
                  const GuiShadow& shadow) const;

  /// Draw @p image over @p rect, its corners at their own size and its
  /// edges and middle stretched, tinted by @p tint.
  void drawNineSlice(const Rect& rect, const GuiNineSlice& image,
                     const GuiColor& tint) const;

  /// Draw a 1px border rectangle.
  void drawBorderRect(const Rect& rect, const GuiColor& color) const;

  /// Parameters for drawing a rounded border rectangle.
  struct RoundedBorderParams {
    /// Rectangle to draw.
    const Rect& rect;
    /// Border color.
    const GuiColor& color;
    /// Corner radius in logical pixels.
    float radius;
    /// Border width in logical pixels.
    float border_width;
  };

  /// Draw a border rectangle with rounded corners and custom width.
  void drawRoundedBorderRect(const RoundedBorderParams& params) const;

  /// Draw text at a specific position with color.
  void drawText(const GuiColor& color, const DrawPos& pos,
                std::string_view str) const;

  /// Draw text centered both horizontally and vertically in a rect.
  void drawCenteredText(const Rect& rect, const GuiColor& color,
                        std::string_view str) const;

  /// Parameters for drawing horizontally centered text.
  struct DrawHCenteredTextParams {
    /// Text color.
    const GuiColor& color;
    /// Center X coordinate in logical pixels.
    int center_x;
    /// Y coordinate in logical pixels.
    int y;
    /// Text string.
    std::string_view str;
  };

  /// Draw text centered horizontally at a given center X and Y.
  void drawHCenteredText(const DrawHCenteredTextParams& params) const;

  /// Measure the width of a text string in logical pixels.
  float measureText(std::string_view str) const;

  /// Return the cap height (ascender) for vertical centering.
  float textCapHeight() const;

  /// Return the full line height for spacing between lines.
  float textLineHeight() const;

  /// Parameters for drawing a textured rectangle.
  struct DrawTexturedRectParams {
    /// Screen rectangle to draw into.
    const Rect& rect;
    /// GPU texture handle to sample.
    RhiTextureHandle texture;
    /// Tint color multiplied with the texture (white = no tint).
    const GuiColor& tint;
  };

  // --- Text in any font -----------------------------------------------
  // UTF-8 throughout; kerned where the font has kerning; each size
  // rasterized at the display's density. The older calls above set the
  // default font, `GuiFont{}`.

  /// Draw one line: @p draw's text from its top-left, in its font.
  void drawText(const GuiTextDraw& draw) const;

  /// How wide @p text is set in @p font, in layout pixels.
  [[nodiscard]] float measureText(std::string_view text,
                                  const GuiFont& font) const;

  /// @p font's ascender, descender and line height (its `line_height`
  /// multiple of the size, or the font's own).
  [[nodiscard]] FontMetrics fontMetrics(const GuiFont& font) const;

  /// @p text broken into lines no wider than @p width: at spaces, inside a
  /// word only when it alone is wider, and at every newline.
  [[nodiscard]] std::vector<GuiWrappedLine>
  wrapText(std::string_view text, const GuiFont& font, float width) const;

  /// @p text, or as much of it as fits @p width with "…" after it.
  [[nodiscard]] std::string ellipsize(std::string_view text,
                                      const GuiFont& font, float width) const;

  /// The loaded face nearest @p font's weight and slant, or `face_id`.
  [[nodiscard]] uint32_t faceFor(const GuiFont& font) const;

  /// Draw a textured rectangle spanning the full UV range [0,1].
  void drawTexturedRect(const DrawTexturedRectParams& params) const;
};

}  // namespace eng
