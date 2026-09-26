#include <algorithm>
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

  constexpr float BORDER_WIDTH = 1.0f;

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


void GuiDrawContext::drawText(const GuiColor& color, const DrawPos& pos,
                              std::string_view str) const {
  drawText(GuiTextDraw{.text = str, .pos = pos, .color = color});
}

const GuiTheme& GuiDrawContext::activeTheme() const {
  return theme != nullptr ? *theme : GuiTheme::dark();
}

GuiDrawContext GuiDrawContext::themedBy(const GuiTheme* scope) const {
  GuiDrawContext scoped = *this;
  if (scope != nullptr) {
    scoped.theme = scope;
  }
  return scoped;
}

float GuiDrawContext::measureText(std::string_view str) const {
  return measureText(str, GuiFont{});
}

float GuiDrawContext::textCapHeight() const {
  return fontMetrics(GuiFont{}).ascender;
}

float GuiDrawContext::textLineHeight() const {
  return fontMetrics(GuiFont{}).line_height;
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
