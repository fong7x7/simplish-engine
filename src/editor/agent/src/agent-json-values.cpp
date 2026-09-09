#include "agent-json-values.h"

#include <editor/agent/agent-names.h>
#include <nlohmann/json.hpp>

namespace eng::editor {

nlohmann::json agentPlacementValue(const EditorPlacement& placement) {
  return {{"asset", placement.asset},
          {"position", agentPointJson(placement.position)},
          {"rotation", agentVec3Json(placement.rotation)}};
}

nlohmann::json agentLightValue(const EditorLight& light) {
  return {{"kind", agentLightKindName(light.kind)},
          {"position", agentPointJson(light.position)},
          {"direction", agentVec3Json(light.direction)},
          {"color", agentVec3Json(light.color)},
          {"intensity", light.intensity},
          {"range", light.range}};
}

}  // namespace eng::editor
