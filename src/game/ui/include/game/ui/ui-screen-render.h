#pragma once

/// @file ui-screen-render.h
/// @brief A game screen drawn to an image, with no window and no GPU.
/// @par Threading
/// Main-thread-only; loads a font and draws on the CPU.

#include <engine/gui/image-data.h>
#include <game/ui/ui-button-info.h>
#include <game/ui/ui-render-size.h>
#include <game/ui/ui-screen.h>
#include <game/ui/ui-values.h>
#include <vector>

namespace eng::game {

/// What `renderUiScreen` made: the picture, where each button was laid
/// out, and whether text could be drawn at all.
struct UiScreenRender {
  /// The screen over a plain stand-in for the game, RGBA.
  ImageData image{};
  /// Every button, where it was laid out.
  std::vector<UiButtonInfo> buttons{};
  /// Whether a font was found; without one, text is laid out but not
  /// drawn.
  bool text = false;
};

/// @p screen, showing @p values, laid out in a view of @p size and drawn
/// by the GUI's software rasterizer, with the system's UI font: what an
/// agent looks at to check a screen it wrote.
[[nodiscard]] UiScreenRender renderUiScreen(const UiScreen& screen,
                                            const UiValues& values,
                                            UiRenderSize size);

}  // namespace eng::game
