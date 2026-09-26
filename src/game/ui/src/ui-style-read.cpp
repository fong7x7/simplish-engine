#include "ui-style-read.h"

namespace eng::game {

void UiStyleRead::bad(std::string_view key, std::string_view want) const {
  uiProblem(read, path,
            "'" + std::string(key) + "' should be " + std::string(want));
}

void UiStyleRead::number(std::string_view key, float& out) const {
  if (const auto found = node.find(key); found != node.end()) {
    if (found->is_number()) {
      out = found->get<float>();
    } else {
      bad(key, "a number");
    }
  }
}

void UiStyleRead::color(std::string_view key,
                        std::optional<GuiColor>& out) const {
  if (!node.contains(key)) {
    return;
  }
  out = parseGuiColor(uiText(node, key));
  if (!out) {
    bad(key, "a colour, #rrggbb or #rrggbbaa");
  }
}

}  // namespace eng::game
