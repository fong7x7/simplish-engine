#pragma once

/// @file gui-palette.h
/// @brief The colours a theme is built from, named for what they are for.
/// @par Threading
/// Immutable value type.

#include "gui-color.h"

namespace eng {

/// A theme's colours, named by role rather than by hue, so a widget asks
/// for `surface` or `primary` and a light theme is a different palette,
/// not different code. `GuiTheme::fromPalette` derives every component
/// style from one of these.
struct GuiPalette {
  /// Behind everything: the window.
  GuiColor background{};
  /// Panels and bars sitting on the background.
  GuiColor surface{};
  /// Things raised off a surface: cards, menus, popovers.
  GuiColor surface_raised{};
  /// Things sunk into a surface: text fields, tracks.
  GuiColor surface_sunken{};
  /// A neutral control's fill: a button at rest.
  GuiColor control{};
  /// A neutral control under the pointer.
  GuiColor control_hover{};
  /// A neutral control held down.
  GuiColor control_pressed{};
  /// Hairlines between areas and round controls.
  GuiColor border{};
  /// A border that has to be seen: a hovered field.
  GuiColor border_strong{};
  /// Body text.
  GuiColor text{};
  /// Secondary text: hints, captions, placeholders.
  GuiColor text_muted{};
  /// Text on a disabled control.
  GuiColor text_disabled{};
  /// The accent: the primary action, the selected item, a focused field.
  GuiColor primary{};
  /// The accent under the pointer.
  GuiColor primary_hover{};
  /// The accent held down.
  GuiColor primary_pressed{};
  /// Text drawn on the accent.
  GuiColor on_primary{};
  /// Destructive actions and errors.
  GuiColor danger{};
  /// A destructive action under the pointer.
  GuiColor danger_hover{};
  /// Success.
  GuiColor success{};
  /// Warnings.
  GuiColor warning{};
  /// The ring round whatever keyboard or pad focus is on.
  GuiColor focus_ring{};
  /// Behind selected text.
  GuiColor selection{};
  /// Dims what is behind a modal.
  GuiColor scrim{};
  /// Drop shadows.
  GuiColor shadow{};
};

/// The dark palette: the editor's, and `GuiTheme::dark()`'s.
inline constexpr GuiPalette GUI_PALETTE_DARK{.background = {30, 30, 34},
                                             .surface = {40, 40, 44},
                                             .surface_raised = {48, 48, 53},
                                             .surface_sunken = {26, 26, 30},
                                             .control = {55, 55, 60},
                                             .control_hover = {70, 70, 75},
                                             .control_pressed = {45, 45, 50},
                                             .border = {55, 55, 59},
                                             .border_strong = {85, 85, 92},
                                             .text = {200, 200, 200},
                                             .text_muted = {120, 120, 120},
                                             .text_disabled = {90, 90, 94},
                                             .primary = {0, 122, 204},
                                             .primary_hover = {0, 140, 230},
                                             .primary_pressed = {0, 104, 176},
                                             .on_primary = {255, 255, 255},
                                             .danger = {180, 60, 60},
                                             .danger_hover = {205, 72, 72},
                                             .success = {70, 170, 100},
                                             .warning = {220, 160, 50},
                                             .focus_ring = {90, 170, 255},
                                             .selection = {50, 100, 200},
                                             .scrim = {0, 0, 0, 160},
                                             .shadow = {0, 0, 0, 110}};

/// The light palette: `GuiTheme::light()`'s.
inline constexpr GuiPalette GUI_PALETTE_LIGHT{
    .background = {240, 240, 242},
    .surface = {250, 250, 251},
    .surface_raised = {255, 255, 255},
    .surface_sunken = {255, 255, 255},
    .control = {226, 226, 230},
    .control_hover = {212, 212, 218},
    .control_pressed = {200, 200, 206},
    .border = {214, 214, 219},
    .border_strong = {170, 170, 178},
    .text = {30, 30, 34},
    .text_muted = {100, 100, 108},
    .text_disabled = {160, 160, 166},
    .primary = {0, 122, 204},
    .primary_hover = {0, 108, 184},
    .primary_pressed = {0, 94, 160},
    .on_primary = {255, 255, 255},
    .danger = {200, 50, 50},
    .danger_hover = {178, 40, 40},
    .success = {40, 150, 80},
    .warning = {200, 130, 20},
    .focus_ring = {0, 122, 204},
    .selection = {51, 153, 255},
    .scrim = {0, 0, 0, 110},
    .shadow = {20, 20, 40, 60}};

}  // namespace eng
