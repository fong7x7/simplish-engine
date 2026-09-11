#include "agent-json-values.h"

#include <editor/agent/agent-names.h>
#include <editor/shell/editor-entity-id.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

nlohmann::json agentPlacementValue(const EditorPlacement& placement) {
  // The id first, and qualified: it is what a level or logic file writes to
  // name this placement, and the index beside it is only good for the rest
  // of this session.
  return {{"id", placement.id},
          {"ref", editorPlacementRef(placement)},
          {"asset", placement.asset},
          {"position", agentPointJson(placement.position)},
          {"rotation", agentVec3Json(placement.rotation)},
          {"collides", placement.collides},
          {"animation", placement.animation}};
}

nlohmann::json agentLightValue(const EditorLight& light) {
  return {{"id", light.id},
          {"ref", editorLightRef(light)},
          {"kind", agentLightKindName(light.kind)},
          {"position", agentPointJson(light.position)},
          {"direction", agentVec3Json(light.direction)},
          {"color", agentVec3Json(light.color)},
          {"intensity", light.intensity},
          {"range", light.range}};
}

nlohmann::json agentPlayerStartValue(const EditorPlayerStart& start) {
  return {{"id", start.id},
          {"ref", editorPlayerStartRef(start)},
          {"player", start.player},
          {"position", agentPointJson(start.position)},
          {"character", start.character}};
}

}  // namespace eng::editor
