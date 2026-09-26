#include "ui-view-widgets.h"

#include <engine/gui/gui-button.h>
#include <engine/gui/gui-checkbox.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-toggle.h>
#include <string>
#include <utility>

namespace eng::game {

void showUiText(GuiWidget& widget, std::string_view text) {
  if (auto* button = dynamic_cast<GuiButton*>(&widget)) {
    button->label = text;
  } else if (auto* label = dynamic_cast<GuiLabel*>(&widget)) {
    label->text = text;
  } else if (auto* box = dynamic_cast<GuiCheckbox*>(&widget)) {
    box->label = std::string(text);
  } else if (auto* toggle = dynamic_cast<GuiToggle*>(&widget)) {
    toggle->label = std::string(text);
  }
}

void setUiChecked(GuiWidget& widget, GuiCheckState state) {
  if (auto* box = dynamic_cast<GuiCheckbox*>(&widget)) {
    box->state = state;
  } else if (auto* toggle = dynamic_cast<GuiToggle*>(&widget)) {
    toggle->on = state == GuiCheckState::CHECKED;
  }
}

bool uiChecked(const GuiWidget& widget) {
  if (const auto* box = dynamic_cast<const GuiCheckbox*>(&widget)) {
    return box->state == GuiCheckState::CHECKED;
  }
  const auto* toggle = dynamic_cast<const GuiToggle*>(&widget);
  return toggle != nullptr && toggle->on;
}

void onUiCheckChange(GuiWidget& widget, std::function<void(bool)> on_change) {
  if (auto* box = dynamic_cast<GuiCheckbox*>(&widget)) {
    box->on_change = std::move(on_change);
  } else if (auto* toggle = dynamic_cast<GuiToggle*>(&widget)) {
    toggle->on_change = std::move(on_change);
  }
}

}  // namespace eng::game
