#pragma once

/// @file ui-json-read.h
/// @brief Reading one screen file: where it is, and what went wrong.
/// @par Threading
/// Pure; one per read.

#include <cstddef>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::game {

/// A screen file being read: its problems so far, and how many nodes it
/// has read, against `UI_SCREEN_MAX_NODES`.
struct UiJsonRead {
  /// Every problem so far, as `path: what`.
  std::vector<std::string> problems{};
  /// Nodes read so far.
  size_t nodes = 0;
  /// How deep the node being read is: 0 for the root.
  size_t depth = 0;
};

/// Note a problem @p what at @p path.
void uiProblem(UiJsonRead& read, std::string_view path, std::string_view what);

/// @p object's @p key, when it is a string; empty otherwise.
[[nodiscard]] std::string uiText(const nlohmann::json& object,
                                 std::string_view key);

/// @p object's @p key, when it is a number; nothing otherwise.
[[nodiscard]] std::optional<float> uiFloat(const nlohmann::json& object,
                                           std::string_view key);

}  // namespace eng::game
