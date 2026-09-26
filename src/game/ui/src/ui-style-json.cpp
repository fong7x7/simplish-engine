#include "ui-style-json.h"

#include "ui-look-json.h"
#include "ui-style-read.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <utility>

namespace eng::game {

namespace {

  /// Every key a node may carry.
  constexpr std::array<std::string_view, 39> NODE_KEYS{
      "type",      "id",           "text",      "action",    "value",
      "max",       "children",     "direction", "gap",       "padding",
      "margin",    "width",        "height",    "min_width", "min_height",
      "max_width", "max_height",   "grow",      "shrink",    "align",
      "justify",   "align_self",   "fill",      "color",     "radius",
      "border",    "border_color", "elevation", "opacity",   "variant",
      "role",      "size",         "weight",    "wrap",      "text_align",
      "visible",   "disabled",     "selected",  "checked"};

  /// Alignments by the word a file writes.
  constexpr std::array<std::pair<std::string_view, Align>, 7> ALIGNS{{
      {"start", Align::START},
      {"center", Align::CENTER},
      {"end", Align::END},
      {"stretch", Align::STRETCH},
      {"space_between", Align::SPACE_BETWEEN},
      {"space_around", Align::SPACE_AROUND},
      {"space_evenly", Align::SPACE_EVENLY},
  }};

  /// Directions by the word a file writes.
  constexpr std::array<std::pair<std::string_view, FlexDirection>, 2>
      DIRECTIONS{
          {{"column", FlexDirection::COLUMN}, {"row", FlexDirection::ROW}}};

  /// One side of a margin: a number, or `"auto"` to take the free space.
  struct Side {
    /// Its width, when a number.
    float px = 0.0F;
    /// Whether it is `auto`.
    bool automatic = false;
  };

  /// @p value as one side, if it is one.
  std::optional<Side> sideOf(const nlohmann::json& value) {
    if (value.is_number()) {
      return Side{.px = value.get<float>()};
    }
    if (value.is_string() && value.get<std::string>() == "auto") {
      return Side{.automatic = true};
    }
    return std::nullopt;
  }

  /// Four sides — top, right, bottom, left — from a single side (all four),
  /// `[vertical, horizontal]`, or `[top, right, bottom, left]`.
  std::optional<std::array<Side, 4>> sidesOf(const nlohmann::json& value) {
    if (const auto all = sideOf(value)) {
      return std::array<Side, 4>{*all, *all, *all, *all};
    }
    if (!value.is_array() || (value.size() != 2 && value.size() != 4)) {
      return std::nullopt;
    }
    std::array<std::optional<Side>, 4> read{};
    for (size_t i = 0; i < 4; ++i) {
      read.at(i) = sideOf(value[value.size() == 2 ? i % 2 : i]);
    }
    if (!std::ranges::all_of(read,
                             [](const auto& s) { return s.has_value(); })) {
      return std::nullopt;
    }
    return std::array<Side, 4>{*read[0], *read[1], *read[2], *read[3]};
  }

  /// @p sides as edges, their `auto` ones as zero.
  Edges edgesOf(const std::array<Side, 4>& sides) {
    return {sides[0].px, sides[1].px, sides[2].px, sides[3].px};
  }

  /// Read the padding into @p style: sides, never `auto`.
  void readPadding(const UiStyleRead& s, UiNodeStyle& style) {
    const auto found = s.node.find("padding");
    if (found == s.node.end()) {
      return;
    }
    const auto sides = sidesOf(*found);
    if (sides && std::ranges::none_of(*sides, &Side::automatic)) {
      style.padding = edgesOf(*sides);
    } else {
      s.bad("padding", "a number, [vertical, horizontal] or "
                       "[top, right, bottom, left]");
    }
  }

  /// Read the margin into @p style: sides, any of them `auto`.
  void readMargin(const UiStyleRead& s, UiNodeStyle& style) {
    const auto found = s.node.find("margin");
    if (found == s.node.end()) {
      return;
    }
    if (const auto sides = sidesOf(*found)) {
      style.margin = edgesOf(*sides);
      style.margin_auto = {(*sides)[0].automatic, (*sides)[1].automatic,
                           (*sides)[2].automatic, (*sides)[3].automatic};
    } else {
      s.bad("margin", "a number or \"auto\", or a list of 2 or 4 of them");
    }
  }

  /// @p text read as `"N%"`: N, when it is a number from 0 up.
  std::optional<float> percentOf(std::string_view text) {
    float value = -1.0F;
    if (!text.ends_with('%')) {
      return std::nullopt;
    }
    const char* end = text.data() + text.size() - 1;
    const auto [at, ec] = std::from_chars(text.data(), end, value);
    return ec == std::errc{} && at == end && value >= 0.0F
               ? std::optional(value)
               : std::nullopt;
  }

  /// Read the size at @p key: pixels into @p px, or `"N%"` into
  /// @p percent.
  void readSize(const UiStyleRead& s, std::string_view key,
                std::pair<float*, float*> out) {
    const auto found = s.node.find(key);
    if (found == s.node.end() || found->is_number()) {
      s.number(key, *out.first);
      return;
    }
    const auto percent = percentOf(uiText(s.node, key));
    if (percent) {
      *out.second = *percent;
    } else {
      s.bad(key, "a number of pixels, or a percentage such as \"50%\"");
    }
  }

  /// Read the sizes into @p style.
  void readSizes(const UiStyleRead& s, UiNodeStyle& style) {
    s.number("gap", style.gap);
    readSize(s, "width", {&style.width, &style.width_percent});
    readSize(s, "height", {&style.height, &style.height_percent});
    s.number("min_width", style.min_width);
    s.number("min_height", style.min_height);
    s.number("max_width", style.max_width);
    s.number("max_height", style.max_height);
    readPadding(s, style);
    readMargin(s, style);
  }

  /// Read the optional factor at @p key into @p out.
  void readFactor(const UiStyleRead& s, std::string_view key,
                  std::optional<float>& out) {
    if (s.node.contains(key)) {
      float factor = 0.0F;
      s.number(key, factor);
      out = factor;
    }
  }

}  // namespace

UiNodeStyle readUiStyle(const nlohmann::json& node, std::string_view path,
                        UiJsonRead& read) {
  const UiStyleRead s{node, path, read};
  UiNodeStyle style;
  s.word("direction", DIRECTIONS, style.direction);
  readSizes(s, style);
  readFactor(s, "grow", style.grow);
  readFactor(s, "shrink", style.shrink);
  s.word("align", ALIGNS, style.align_items);
  s.word("justify", ALIGNS, style.justify);
  s.word("align_self", ALIGNS, style.align_self);
  readUiLook(s, style);
  return style;
}

bool knownUiNodeKey(std::string_view key) {
  return std::ranges::find(NODE_KEYS, key) != NODE_KEYS.end();
}

}  // namespace eng::game
