#include "engine/gui/gui-style.h"

namespace eng {

namespace {

  // Algorithm: Static dark-theme palette for editor and in-game GUI defaults.
  const GuiStyle K_DARK_STYLE{
      // Global
      .background = {30, 30, 34},
      .panel = {40, 40, 44},
      .border = {55, 55, 59},
      .overlay = {0, 0, 0, 180},
      .text = {200, 200, 200},
      .text_dim = {120, 120, 120},
      .error = {220, 60, 60},
      // Button
      .btn_bg = {55, 55, 60},
      .btn_bg_hover = {70, 70, 75},
      .btn_text = {200, 200, 200},
      .btn_accent_bg = {0, 122, 204},
      .btn_accent_bg_hover = {0, 140, 230},
      .btn_danger_bg = {180, 60, 60},
      .btn_corner_radius = 5.0f,
      // Text Input
      .input_bg = {30, 30, 34},
      .input_text = {200, 200, 200},
      .input_border = {55, 55, 59},
      .input_border_focused = {0, 122, 204},
      .input_selection_bg = {50, 100, 200},
      .input_selection_text = {255, 255, 255},
      .input_corner_radius = 0.0f,
      // Dropdown
      .dropdown_bg = {40, 40, 44},
      .dropdown_text = {200, 200, 200},
      .dropdown_hover = {50, 50, 55},
      .dropdown_width = 140,
      .dropdown_item_height = 24,
      // Slider
      .slider_track = {55, 55, 60},
      .slider_fill = {0, 122, 204},
      .slider_handle = {200, 200, 200},
      .slider_handle_hover = {230, 230, 230},
      .slider_handle_size = 12.0f,
      .slider_track_height = 4.0f,
      // Hover
      .hover_highlight = {50, 50, 55},
  };

  // Algorithm: Static light-theme palette mirroring dark structure.
  const GuiStyle K_LIGHT_STYLE{
      // Global
      .background = {240, 240, 242},
      .panel = {255, 255, 255},
      .border = {200, 200, 204},
      .overlay = {0, 0, 0, 120},
      .text = {30, 30, 34},
      .text_dim = {100, 100, 108},
      .error = {200, 40, 40},
      // Button
      .btn_bg = {220, 220, 224},
      .btn_bg_hover = {200, 200, 206},
      .btn_text = {30, 30, 34},
      .btn_accent_bg = {0, 122, 204},
      .btn_accent_bg_hover = {0, 105, 180},
      .btn_danger_bg = {200, 50, 50},
      .btn_corner_radius = 5.0f,
      // Text Input
      .input_bg = {255, 255, 255},
      .input_text = {30, 30, 34},
      .input_border = {180, 180, 186},
      .input_border_focused = {0, 122, 204},
      .input_selection_bg = {51, 153, 255},
      .input_selection_text = {255, 255, 255},
      .input_corner_radius = 0.0f,
      // Dropdown
      .dropdown_bg = {255, 255, 255},
      .dropdown_text = {30, 30, 34},
      .dropdown_hover = {230, 235, 245},
      .dropdown_width = 140,
      .dropdown_item_height = 24,
      // Slider
      .slider_track = {200, 200, 204},
      .slider_fill = {0, 122, 204},
      .slider_handle = {80, 80, 88},
      .slider_handle_hover = {50, 50, 58},
      .slider_handle_size = 12.0f,
      .slider_track_height = 4.0f,
      // Hover
      .hover_highlight = {225, 230, 240},
  };

}  // namespace

const GuiStyle& GuiStyle::dark() {
  return K_DARK_STYLE;
}

const GuiStyle& GuiStyle::light() {
  return K_LIGHT_STYLE;
}

}  // namespace eng
