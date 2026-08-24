#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-rect.h>
#include <engine/gui/gui-renderer.h>
#include <engine/gui/text-pipeline.h>
#include <string_view>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

namespace eng {
namespace {

  constexpr float DEBUG_CHAR_W = 8.0f;
  constexpr float DEFAULT_FONT_SIZE = 14.0f;
  constexpr float BORDER_WIDTH = 1.0f;

  const FontFace* getFace(const GuiDrawContext& ctx) {
    if (ctx.text_pipeline == nullptr) {
      return nullptr;
    }
    for (const auto& face : ctx.text_pipeline->faces) {
      if (face.face_id == ctx.face_id) {
        return &face;
      }
    }
    return nullptr;
  }

  /// Parameters for batched glyph emission.
  struct BatchedGlyphParams {
    /// Draw context.
    const GuiDrawContext& ctx;
    /// Starting pen X position.
    float pen_x;
    /// Pen Y position (baseline).
    float pen_y;
    /// Text to render.
    std::string_view str;
  };

  /// Parameters for advancing a single glyph.
  struct GlyphAdvanceParams {
    /// Draw context.
    const GuiDrawContext& ctx;
    /// Pen Y position (baseline).
    float pen_y;
    /// Packed RGBA color.
    uint32_t packed;
  };

  float advanceGlyph(const GlyphAdvanceParams& g, float pen_x,
                     uint32_t codepoint) {
    const auto* glyph =
        g.ctx.text_pipeline->ensureGlyph(g.ctx.face_id, codepoint);
    if (glyph != nullptr) {
      g.ctx.renderer->emitGlyph({pen_x, g.pen_y, *glyph, 1.0f, g.packed});
      return pen_x + glyph->advance;
    }
    return pen_x + DEBUG_CHAR_W;
  }

  float emitBatchedGlyphs(const BatchedGlyphParams& p, uint32_t packed) {
    GlyphAdvanceParams g{p.ctx, p.pen_y, packed};
    float pen_x = p.pen_x;
    for (char ch : p.str) {
      auto codepoint = static_cast<uint32_t>(static_cast<unsigned char>(ch));
      pen_x = advanceGlyph(g, pen_x, codepoint);
    }
    return pen_x;
  }

  /// Parameters for placeholder glyph emission.
  struct PlaceholderGlyphParams {
    /// Renderer context.
    GuiRendererContext& renderer;
    /// Starting pen X position.
    float pen_x;
    /// Pen Y position.
    float pen_y;
    /// Text to render.
    std::string_view str;
  };

  void emitPlaceholderGlyphs(const PlaceholderGlyphParams& p, uint32_t packed) {
    auto& renderer = p.renderer;
    float pen_x = p.pen_x;
    float pen_y = p.pen_y;
    for (char ch : p.str) {
      (void)ch;
      Rect glyph_rect{pen_x, pen_y, DEBUG_CHAR_W - 1.0f, DEBUG_CHAR_W};
      renderer.emitQuad({glyph_rect, packed, 0.0f, 0.0f});
      pen_x += DEBUG_CHAR_W;
    }
  }

}  // namespace

void GuiDrawContext::drawFilledRect(const Rect& rect,
                                    const GuiColor& color) const {
  if (renderer == nullptr) {
    return;
  }
  renderer->emitQuad({rect, color.pack(), 0.0f, 0.0f});
}

void GuiDrawContext::drawRoundedRect(const Rect& rect, const GuiColor& color,
                                     float radius) const {
  if (renderer == nullptr) {
    return;
  }
  renderer->emitQuad({rect, color.pack(), radius, 0.0f});
}

void GuiDrawContext::drawBorderRect(const Rect& rect,
                                    const GuiColor& color) const {
  if (renderer == nullptr) {
    return;
  }
  renderer->emitQuad({rect, color.pack(), 0.0f, BORDER_WIDTH});
}

void GuiDrawContext::drawRoundedBorderRect(
    const RoundedBorderParams& params) const {
  if (renderer == nullptr) {
    return;
  }
  renderer->emitQuad(
      {params.rect, params.color.pack(), params.radius, params.border_width});
}

void GuiDrawContext::emitTextGlyphs(const DrawPos& pos, std::string_view str,
                                    uint32_t packed) const {
  float pen_y = pos.y;
  const auto* face = getFace(*this);
  if (face != nullptr) {
    pen_y += face->ascender;
    emitBatchedGlyphs({*this, pos.x, pen_y, str}, packed);
  } else {
    emitPlaceholderGlyphs({*renderer, pos.x, pen_y, str}, packed);
  }
}

void GuiDrawContext::drawText(const GuiColor& color, const DrawPos& pos,
                              std::string_view str) const {
  if (renderer == nullptr || str.empty()) {
    return;
  }
  uint32_t packed = color.pack();
  if (text_pipeline != nullptr) {
    emitTextGlyphs(pos, str, packed);
    return;
  }
  emitPlaceholderGlyphs({*renderer, pos.x, pos.y, str}, packed);
}

float GuiDrawContext::measureText(std::string_view str) const {
  if (str.empty()) {
    return 0.0f;
  }
  if (text_pipeline == nullptr) {
    return static_cast<float>(str.size()) * DEBUG_CHAR_W;
  }

  float width = 0.0f;
  for (char ch : str) {
    auto codepoint = static_cast<uint32_t>(static_cast<unsigned char>(ch));
    const auto* glyph = text_pipeline->ensureGlyph(face_id, codepoint);
    width += (glyph != nullptr) ? glyph->advance : DEBUG_CHAR_W;
  }
  return width;
}

float GuiDrawContext::textCapHeight() const {
  const auto* face = getFace(*this);
  if (face != nullptr && face->ascender > 0.0f) {
    return face->ascender;
  }
  return DEFAULT_FONT_SIZE;
}

float GuiDrawContext::textLineHeight() const {
  const auto* face = getFace(*this);
  if (face != nullptr && face->line_height > 0.0f) {
    return face->line_height;
  }
  return DEFAULT_FONT_SIZE;
}

void GuiDrawContext::drawCenteredText(const Rect& rect, const GuiColor& color,
                                      std::string_view str) const {
  float tw = measureText(str);
  float lh = textLineHeight();
  float tx = rect.x + (rect.w - tw) / 2.0f;
  float ty = rect.y + (rect.h - lh) / 2.0f;
  drawText(color, {tx, ty}, str);
}

void GuiDrawContext::drawHCenteredText(
    const DrawHCenteredTextParams& params) const {
  float tw = measureText(params.str);
  float tx = static_cast<float>(params.center_x) - tw / 2.0f;
  drawText(params.color, {tx, static_cast<float>(params.y)}, params.str);
}

void GuiDrawContext::drawTexturedRect(
    const DrawTexturedRectParams& params) const {
  if (renderer == nullptr) {
    return;
  }
  const Rect full_uv{0.0f, 0.0f, 1.0f, 1.0f};
  renderer->emitTexturedQuad(
      {params.rect, full_uv, params.texture, params.tint.pack()});
}

void setUiCursor(GuiCursorShape shape) {
  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
  // — Justified: SDL cursor handles must outlive SDL_SetCursor; caching
  //   static cursors avoids per-frame create/destroy and keeps the active
  //   cursor alive.  Thread-safe because GUI runs on the main thread only.
  static SDL_Cursor* arrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
  static SDL_Cursor* pointer =
      SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
  static SDL_Cursor* ibeam = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
  SDL_Cursor* cursor = arrow;
  if (shape == GuiCursorShape::POINTER) {
    cursor = pointer;
  } else if (shape == GuiCursorShape::IBEAM) {
    cursor = ibeam;
  }
  SDL_SetCursor(cursor);
}

}  // namespace eng
