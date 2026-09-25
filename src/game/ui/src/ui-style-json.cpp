#include "ui-style-json.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <optional>
#include <string>
#include <utility>

namespace eng::game {

namespace {

  /// Every key a node may carry.
  constexpr std::array<std::string_view, 22> NODE_KEYS{
      "type",       "id",        "text",       "action",  "value",  "max",
      "children",   "direction", "gap",        "padding", "margin", "width",
      "height",     "min_width", "min_height", "grow",    "align",  "justify",
      "align_self", "fill",      "color",      "radius"};

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

  /// The alignment @p word names, if any.
  std::optional<Align> alignNamed(std::string_view word) {
    const auto* found = std::ranges::find(
        ALIGNS, word, &std::pair<std::string_view, Align>::first);
    return found != ALIGNS.end() ? std::optional(found->second) : std::nullopt;
  }

  /// Two hex digits of @p text from @p at, as a byte.
  std::optional<uint8_t> hexByte(std::string_view text, size_t at) {
    uint8_t value = 0;
    const char* first = text.data() + at;
    const auto [end, ec] = std::from_chars(first, first + 2, value, 16);
    return ec == std::errc{} && end == first + 2 ? std::optional(value)
                                                 : std::nullopt;
  }

  /// The colour `#rrggbb` or `#rrggbbaa` names, if it is one.
  std::optional<GuiColor> colorNamed(std::string_view text) {
    if ((text.size() != 7 && text.size() != 9) || text.front() != '#') {
      return std::nullopt;
    }
    const auto r = hexByte(text, 1);
    const auto g = hexByte(text, 3);
    const auto b = hexByte(text, 5);
    const auto a =
        text.size() == 9 ? hexByte(text, 7) : std::optional<uint8_t>(255);
    if (!r || !g || !b || !a) {
      return std::nullopt;
    }
    return GuiColor{*r, *g, *b, *a};
  }

  /// Edges from a number (all four) or `[top, right, bottom, left]`, or
  /// `[vertical, horizontal]`.
  std::optional<Edges> edgesOf(const nlohmann::json& value) {
    if (value.is_number()) {
      const auto all = value.get<float>();
      return Edges{all, all, all, all};
    }
    if (!value.is_array() || (value.size() != 2 && value.size() != 4) ||
        !std::ranges::all_of(value,
                             [](const auto& v) { return v.is_number(); })) {
      return std::nullopt;
    }
    const auto at = [&value](size_t i) {
      return value[i].get<float>();
    };
    return value.size() == 2 ? Edges{at(0), at(1), at(0), at(1)}
                             : Edges{at(0), at(1), at(2), at(3)};
  }

  /// A style read: the node, where it is, and where problems go.
  struct StyleRead {
    /// The node's JSON.
    const nlohmann::json& node;
    /// Where it is.
    std::string_view path;
    /// Where problems go.
    UiJsonRead& read;
  };

  /// Note that @p key's value did not read, saying what it should be.
  void badValue(const StyleRead& s, std::string_view key,
                std::string_view want) {
    uiProblem(s.read, s.path,
              "'" + std::string(key) + "' should be " + std::string(want));
  }

  /// Read the number at @p key into @p out, when there is one.
  void readNumber(const StyleRead& s, std::string_view key, float& out) {
    if (const auto found = s.node.find(key); found != s.node.end()) {
      if (found->is_number()) {
        out = found->get<float>();
      } else {
        badValue(s, key, "a number");
      }
    }
  }

  /// Read the edges at @p key into @p out, when there are some.
  void readEdges(const StyleRead& s, std::string_view key, Edges& out) {
    if (const auto found = s.node.find(key); found != s.node.end()) {
      if (const auto edges = edgesOf(*found)) {
        out = *edges;
      } else {
        badValue(s, key, "a number, or [top, right, bottom, left]");
      }
    }
  }

  /// Read the alignment at @p key into @p out, when there is one.
  void readAlign(const StyleRead& s, std::string_view key, Align& out) {
    if (!s.node.contains(key)) {
      return;
    }
    if (const auto align = alignNamed(uiText(s.node, key))) {
      out = *align;
    } else {
      badValue(s, key,
               "start, center, end, stretch, space_between, "
               "space_around or space_evenly");
    }
  }

  /// Read the colour at @p key into @p out, when there is one.
  void readColor(const StyleRead& s, std::string_view key,
                 std::optional<GuiColor>& out) {
    if (!s.node.contains(key)) {
      return;
    }
    out = colorNamed(uiText(s.node, key));
    if (!out) {
      badValue(s, key, "a colour, #rrggbb or #rrggbbaa");
    }
  }

  /// Read `direction` into @p style.
  void readDirection(const StyleRead& s, UiNodeStyle& style) {
    if (!s.node.contains("direction")) {
      return;
    }
    const std::string word = uiText(s.node, "direction");
    if (word == "row" || word == "column") {
      style.direction =
          word == "row" ? FlexDirection::ROW : FlexDirection::COLUMN;
    } else {
      badValue(s, "direction", "row or column");
    }
  }

  /// Read the sizes into @p style.
  void readSizes(const StyleRead& s, UiNodeStyle& style) {
    readNumber(s, "gap", style.gap);
    readNumber(s, "width", style.width);
    readNumber(s, "height", style.height);
    readNumber(s, "min_width", style.min_width);
    readNumber(s, "min_height", style.min_height);
    readNumber(s, "radius", style.radius);
    readEdges(s, "padding", style.padding);
    readEdges(s, "margin", style.margin);
  }

}  // namespace

UiNodeStyle readUiStyle(const nlohmann::json& node, std::string_view path,
                        UiJsonRead& read) {
  const StyleRead s{node, path, read};
  UiNodeStyle style;
  readDirection(s, style);
  readSizes(s, style);
  if (const auto found = node.find("grow"); found != node.end()) {
    float grow = 0.0F;
    readNumber(s, "grow", grow);
    style.grow = grow;
  }
  readAlign(s, "align", style.align_items);
  readAlign(s, "justify", style.justify);
  readAlign(s, "align_self", style.align_self);
  readColor(s, "fill", style.fill);
  readColor(s, "color", style.color);
  return style;
}

bool knownUiNodeKey(std::string_view key) {
  return std::ranges::find(NODE_KEYS, key) != NODE_KEYS.end();
}

}  // namespace eng::game
