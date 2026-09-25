#include "ui-json-read.h"
#include "ui-style-json.h"

#include <algorithm>
#include <array>
#include <game/ui/ui-screen-json.h>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace eng::game {

namespace {

  using json = nlohmann::json;

  /// Node kinds by the `type` a file writes.
  constexpr std::array<std::pair<std::string_view, UiNodeKind>, 5> KINDS{{
      {"panel", UiNodeKind::PANEL},
      {"label", UiNodeKind::LABEL},
      {"button", UiNodeKind::BUTTON},
      {"bar", UiNodeKind::BAR},
      {"spacer", UiNodeKind::SPACER},
  }};

  /// Anchors by the word a file writes.
  constexpr std::array<std::pair<std::string_view, UiAnchor>, 10> ANCHORS{{
      {"center", UiAnchor::CENTER},
      {"top", UiAnchor::TOP},
      {"bottom", UiAnchor::BOTTOM},
      {"left", UiAnchor::LEFT},
      {"right", UiAnchor::RIGHT},
      {"top_left", UiAnchor::TOP_LEFT},
      {"top_right", UiAnchor::TOP_RIGHT},
      {"bottom_left", UiAnchor::BOTTOM_LEFT},
      {"bottom_right", UiAnchor::BOTTOM_RIGHT},
      {"fill", UiAnchor::FILL},
  }};

  /// What @p word names in @p table, if anything.
  template <typename Value, size_t N>
  std::optional<Value>
  named(const std::array<std::pair<std::string_view, Value>, N>& table,
        std::string_view word) {
    const auto found = std::ranges::find(
        table, word, &std::pair<std::string_view, Value>::first);
    return found != table.end() ? std::optional(found->second) : std::nullopt;
  }

  /// @p node's `max`: a key, or a number written as one.
  std::string maxOf(const json& node) {
    const auto found = node.find("max");
    if (found != node.end() && found->is_number()) {
      return std::to_string(found->get<float>());
    }
    return uiText(node, "max");
  }

  /// Note each key of @p node a node may not carry.
  void checkKeys(const json& node, std::string_view path, UiJsonRead& read) {
    for (const auto& [key, value] : node.items()) {
      if (!knownUiNodeKey(key)) {
        uiProblem(read, path, "unknown key '" + key + "'");
      }
    }
  }

  /// Note what @p node lacks that its kind needs.
  void checkNeeds(const UiNode& node, std::string_view path, UiJsonRead& read) {
    if (node.kind == UiNodeKind::BUTTON && node.action.empty()) {
      uiProblem(read, path, "a button needs an 'action'");
    }
    if (node.kind == UiNodeKind::BAR &&
        (node.value.empty() || node.max.empty())) {
      uiProblem(read, path, "a bar needs a 'value' and a 'max'");
    }
  }

  std::optional<UiNode> readNode(const json& node, const std::string& path,
                                 UiJsonRead& read);

  /// Read @p node's `children` into @p made, a level deeper.
  void readChildren(const json& node, const std::string& path, UiJsonRead& read,
                    UiNode& made) {
    const auto found = node.find("children");
    if (found == node.end()) {
      return;
    }
    if (!found->is_array() || made.kind != UiNodeKind::PANEL) {
      uiProblem(read, path, "only a panel has children, as a list");
      return;
    }
    ++read.depth;
    for (size_t i = 0; i < found->size(); ++i) {
      const std::string at = path + "/children[" + std::to_string(i) + "]";
      if (auto child = readNode((*found)[i], at, read)) {
        made.children.push_back(std::move(*child));
      }
    }
    --read.depth;
  }

  /// The node of @p kind @p node describes at @p path, but for its
  /// children.
  UiNode nodeOf(const json& node, UiNodeKind kind, std::string_view path,
                UiJsonRead& read) {
    return {.kind = kind,
            .id = uiText(node, "id"),
            .text = uiText(node, "text"),
            .action = uiText(node, "action"),
            .value = uiText(node, "value"),
            .max = maxOf(node),
            .style = readUiStyle(node, path, read)};
  }

  /// Whether a node at @p path may be read: the limits.
  bool withinLimits(const std::string& path, UiJsonRead& read) {
    if (read.depth >= UI_SCREEN_MAX_DEPTH) {
      uiProblem(read, path, "nested too deep");
      return false;
    }
    if (++read.nodes > UI_SCREEN_MAX_NODES) {
      uiProblem(read, path, "more nodes than a screen may hold");
      return false;
    }
    return true;
  }

  /// The node @p node describes, at @p path; nothing when it cannot be.
  std::optional<UiNode> readNode(const json& node, const std::string& path,
                                 UiJsonRead& read) {
    const auto kind =
        node.is_object() ? named(KINDS, uiText(node, "type")) : std::nullopt;
    if (!kind) {
      uiProblem(read, path,
                "a node is an object whose 'type' is panel, "
                "label, button, bar or spacer");
      return std::nullopt;
    }
    if (!withinLimits(path, read)) {
      return std::nullopt;
    }
    UiNode made = nodeOf(node, *kind, path, read);
    checkKeys(node, path, read);
    checkNeeds(made, path, read);
    readChildren(node, path, read, made);
    return made;
  }

  /// Read @p file's `layer`, `anchor` and `inset` into @p screen.
  void readPlacement(const json& file, UiJsonRead& read, UiScreen& screen) {
    const std::string layer = uiText(file, "layer");
    if (layer == "hud" || layer == "menu" || layer.empty()) {
      screen.layer = layer == "hud" ? UiScreenLayer::HUD : UiScreenLayer::MENU;
    } else {
      uiProblem(read, "layer", "should be menu or hud");
    }
    if (file.contains("anchor")) {
      const auto anchor = named(ANCHORS, uiText(file, "anchor"));
      screen.anchor = anchor.value_or(UiAnchor::CENTER);
      if (!anchor) {
        uiProblem(read, "anchor",
                  "should be center, top, bottom, left, "
                  "right, a corner, or fill");
      }
    }
    screen.inset = uiFloat(file, "inset").value_or(screen.inset);
  }

}  // namespace

UiScreenRead parseUiScreen(std::string_view text, std::string_view id) {
  const json file = json::parse(text, nullptr, false);
  if (file.is_discarded() || !file.is_object()) {
    return {std::nullopt, {"not a JSON object"}};
  }
  UiJsonRead read;
  const std::string schema = uiText(file, "schema");
  if (!schema.empty() && schema != UI_SCREEN_SCHEMA) {
    uiProblem(read, "schema", "should be " + std::string(UI_SCREEN_SCHEMA));
  }
  UiScreen screen{.id = std::string(id)};
  readPlacement(file, read, screen);
  auto root = file.contains("root") ? readNode(file["root"], "root", read)
                                    : std::nullopt;
  if (!root) {
    uiProblem(read, "root", "a screen needs a root node");
    return {std::nullopt, std::move(read.problems)};
  }
  screen.root = std::move(*root);
  return {std::move(screen), std::move(read.problems)};
}

}  // namespace eng::game
