#pragma once

/// @file ui-style-read.h
/// @brief One node's style keys being read: the node, where it is, and
/// the readers every style key shares.
/// @par Threading
/// Pure; one per node read.

#include "ui-json-read.h"

#include <algorithm>
#include <array>
#include <engine/gui/gui-color.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace eng::game {

/// A style read: the node, where it is, and where problems go.
struct UiStyleRead {
  /// The node's JSON.
  const nlohmann::json& node;
  /// Where it is.
  std::string_view path;
  /// Where problems go.
  UiJsonRead& read;

  /// Note that @p key's value did not read, saying what it should be.
  void bad(std::string_view key, std::string_view want) const;
  /// Read the number at @p key into @p out, when there is one.
  void number(std::string_view key, float& out) const;
  /// Read the colour at @p key into @p out, when there is one.
  void color(std::string_view key, std::optional<GuiColor>& out) const;

  /// Read the word at @p key into @p out, when there is one, from
  /// @p table; a problem naming every word when it names none.
  template <typename Value, size_t N>
  void word(std::string_view key,
            const std::array<std::pair<std::string_view, Value>, N>& table,
            Value& out) const {
    if (!node.contains(key)) {
      return;
    }
    const std::string said = uiText(node, key);
    const auto* found = std::ranges::find(
        table, said, &std::pair<std::string_view, Value>::first);
    if (found != table.end()) {
      out = found->second;
      return;
    }
    std::string want = "one of";
    for (const auto& [name, value] : table) {
      want += (&name == &table.front().first ? " " : ", ") + std::string(name);
    }
    bad(key, want);
  }
};

}  // namespace eng::game
