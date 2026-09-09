#pragma once

/// @file agent-json-values.h
/// @brief Shared JSON shapes for the agent API's own serialisers.
/// @par Threading Thread-safe (pure functions over value types).

#include <editor/shell/editor-light.h>
#include <editor/shell/editor-placement.h>
#include <editor/shell/iso-projection.h>
#include <engine/math/vec3.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

/// A world position, always these three keys in this order.
[[nodiscard]] inline nlohmann::json agentPointJson(WorldPoint point) {
  return {{"x", point.x}, {"y", point.y}, {"z", point.z}};
}

/// A direction or a colour: three components under the same keys, so a
/// reader that can take one apart can take the other apart too.
[[nodiscard]] inline nlohmann::json agentVec3Json(const Vec3& vec) {
  return {{"x", vec.x}, {"y", vec.y}, {"z", vec.z}};
}

/// One placement, without the asset name only the shell state can supply.
[[nodiscard]] nlohmann::json
agentPlacementValue(const EditorPlacement& placement);

/// One light, every field of it, whether or not its kind uses them all —
/// what a kind ignores is said in the schema, and a hole in the record
/// would be worse than a number nothing reads.
[[nodiscard]] nlohmann::json agentLightValue(const EditorLight& light);

}  // namespace eng::editor
