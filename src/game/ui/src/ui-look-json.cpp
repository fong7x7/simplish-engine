#include "ui-look-json.h"

#include <array>
#include <utility>

namespace eng::game {

namespace {

  /// Button variants by the word a file writes.
  constexpr std::array<std::pair<std::string_view, GuiButtonVariant>, 4>
      VARIANTS{{{"neutral", GuiButtonVariant::NEUTRAL},
                {"primary", GuiButtonVariant::PRIMARY},
                {"danger", GuiButtonVariant::DANGER},
                {"ghost", GuiButtonVariant::GHOST}}};

  /// Elevations by the word a file writes.
  constexpr std::array<std::pair<std::string_view, GuiElevation>, 4> ELEVATIONS{
      {{"none", GuiElevation::NONE},
       {"low", GuiElevation::LOW},
       {"mid", GuiElevation::MID},
       {"high", GuiElevation::HIGH}}};

  /// Text roles by the word a file writes.
  constexpr std::array<std::pair<std::string_view, GuiTextRole>, 6> ROLES{{
      {"caption", GuiTextRole::CAPTION},
      {"label", GuiTextRole::LABEL},
      {"body", GuiTextRole::BODY},
      {"heading", GuiTextRole::HEADING},
      {"title", GuiTextRole::TITLE},
      {"display", GuiTextRole::DISPLAY},
  }};

  /// Text alignments by the word a file writes.
  constexpr std::array<std::pair<std::string_view, GuiLabelAlign>, 3>
      TEXT_ALIGNS{{{"left", GuiLabelAlign::LEFT},
                   {"center", GuiLabelAlign::CENTER},
                   {"right", GuiLabelAlign::RIGHT}}};

  /// The lightest and boldest weight a file may ask for.
  constexpr float WEIGHT_MIN = 100.0F;
  /// The boldest.
  constexpr float WEIGHT_MAX = 900.0F;

  /// Read `role` into @p style, when there is one.
  void readRole(const UiStyleRead& s, UiNodeStyle& style) {
    if (s.node.contains("role")) {
      GuiTextRole role = GuiTextRole::BODY;
      s.word("role", ROLES, role);
      style.role = role;
    }
  }

  /// Read `weight` into @p style: 100 to 900.
  void readWeight(const UiStyleRead& s, UiNodeStyle& style) {
    float weight = 0.0F;
    s.number("weight", weight);
    if (weight == 0.0F) {
      return;
    }
    if (weight < WEIGHT_MIN || weight > WEIGHT_MAX) {
      s.bad("weight", "from 100 to 900");
      return;
    }
    style.weight = static_cast<uint16_t>(weight);
  }

  /// Read `wrap` into @p style: true to wrap at the label's width.
  void readWrap(const UiStyleRead& s, UiNodeStyle& style) {
    const auto found = s.node.find("wrap");
    if (found == s.node.end()) {
      return;
    }
    if (found->is_boolean()) {
      style.wrap = found->get<bool>() ? GuiTextWrap::WORD : GuiTextWrap::NONE;
    } else {
      s.bad("wrap", "true or false");
    }
  }

  /// Read the text keys into @p style.
  void readText(const UiStyleRead& s, UiNodeStyle& style) {
    readRole(s, style);
    s.number("size", style.text_size);
    readWeight(s, style);
    readWrap(s, style);
    s.word("text_align", TEXT_ALIGNS, style.text_align);
    s.word("variant", VARIANTS, style.variant);
  }

  /// Read the binding at @p key: a key, or `!key`.
  std::string readBinding(const UiStyleRead& s, std::string_view key) {
    const auto found = s.node.find(key);
    if (found == s.node.end()) {
      return {};
    }
    const std::string bound = uiText(s.node, key);
    if (bound.empty() || bound == "!") {
      s.bad(key, "a value's key, or !key");
      return {};
    }
    return bound;
  }

}  // namespace

void readUiLook(const UiStyleRead& s, UiNodeStyle& style) {
  s.color("fill", style.fill);
  s.color("color", style.color);
  s.color("border_color", style.border_color);
  s.number("radius", style.radius);
  s.number("border", style.border);
  s.word("elevation", ELEVATIONS, style.elevation);
  s.number("opacity", style.opacity);
  if (style.opacity < 0.0F || style.opacity > 1.0F) {
    s.bad("opacity", "from 0 to 1");
    style.opacity = 1.0F;
  }
  readText(s, style);
}

UiBindings readUiBindings(const UiStyleRead& s) {
  return {.visible = readBinding(s, "visible"),
          .disabled = readBinding(s, "disabled"),
          .selected = readBinding(s, "selected"),
          .checked = readBinding(s, "checked")};
}

}  // namespace eng::game
