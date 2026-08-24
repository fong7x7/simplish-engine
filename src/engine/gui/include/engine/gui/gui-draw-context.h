#pragma once

/// @file gui-draw-context.h
/// @brief Immutable handles for drawing text and rects through the GUI RHI
/// path.
/// @par Threading Main thread only.

#include "draw-pos.h"
#include "gui-color.h"
#include "gui-renderer.h"
#include "text-pipeline.h"

#include <cstdint>
#include <string_view>

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

  /// Draw a solid-filled rectangle.
  void drawFilledRect(const Rect& rect, const GuiColor& color) const;

  /// Draw a filled rectangle with rounded corners.
  void drawRoundedRect(const Rect& rect, const GuiColor& color,
                       float radius) const;

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

  /// Draw a textured rectangle spanning the full UV range [0,1].
  void drawTexturedRect(const DrawTexturedRectParams& params) const;

private:
  /// Emit glyphs using the text pipeline (batched or placeholder fallback).
  void emitTextGlyphs(const DrawPos& pos, std::string_view str,
                      uint32_t packed) const;
};

}  // namespace eng
