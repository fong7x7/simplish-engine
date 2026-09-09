#pragma once

/// @file agent-param-type.h
/// @brief The sort of value one tool parameter takes.
/// @par Threading Thread-safe (immutable value type).

#include <cstdint>
#include <string_view>

namespace eng::editor {

/// What a tool parameter accepts.
///
/// Deliberately a short list. The MCP bridge turns each of these into a
/// JSON Schema type, and a vocabulary small enough to map in one table is
/// one nothing can map wrongly.
/// @thread_safety Immutable value type.
enum class AgentParamType : uint8_t {
  /// Any number: a distance in tiles, an angle in degrees, a multiplier.
  NUMBER,
  /// A whole number, and never negative: an index into one of the
  /// editor's lists.
  INTEGER,
  /// Text: a name, a path, or one of a fixed set of words.
  STRING,
  /// An asset, named either by its index in the scanned list or by its
  /// name or path relative to the assets root. Both are accepted because
  /// an agent that has just read `list_assets` has the index, and one
  /// working from what a person said has the name.
  ASSET_REF,
};

/// The word this type is published as, which the MCP bridge maps to a JSON
/// Schema type.
[[nodiscard]] constexpr std::string_view
agentParamTypeName(AgentParamType type) {
  switch (type) {
    case AgentParamType::NUMBER:
      return "number";
    case AgentParamType::INTEGER:
      return "integer";
    case AgentParamType::STRING:
      return "string";
    case AgentParamType::ASSET_REF:
      return "asset_ref";
  }
  return "string";
}

}  // namespace eng::editor
