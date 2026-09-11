#include <algorithm>
#include <cmath>
#include <editor/shell/editor-actor-placement.h>
#include <editor/shell/editor-behavior-choices.h>
#include <editor/shell/editor-placement-transform.h>

namespace eng::editor {

namespace {

  /// Degrees in a radian. Presentation-side trigonometry: the editor draws
  /// with libm, and what reaches the simulation is the authored degrees.
  constexpr float DEGREES_PER_RADIAN = 57.2957795130823208768f;

  /// @p placement's box as it would be unturned, which is what its size is
  /// measured from: turning a prop does not make it wider.
  PlacementBounds unturnedBounds(const EditorPlacement& placement,
                                 const EditorAsset& asset) {
    EditorPlacement upright = placement;
    upright.rotation = {};
    return placementWorldBounds(asset, upright);
  }

}  // namespace

bool isEditorActor(const EditorPlacement& placement) {
  return !placement.behavior.empty();
}

std::vector<size_t> editorActorPlacements(const EditorDocument& document) {
  std::vector<size_t> actors;
  for (size_t i = 0; i < document.placements.size(); ++i) {
    if (isEditorActor(document.placements[i])) {
      actors.push_back(i);
    }
  }
  return actors;
}

std::vector<std::string> editorActorIds(const EditorDocument& document) {
  std::vector<std::string> ids;
  for (const size_t index : editorActorPlacements(document)) {
    ids.push_back(document.placements[index].id);
  }
  return ids;
}

float editorActorYawDegrees(const EditorPlacement& placement) {
  return placement.rotation.z + EDITOR_MODEL_FRONT_DEGREES;
}

game::ActorSpawn editorActorSpawn(const EditorPlacement& placement,
                                  const EditorAsset& asset) {
  const PlacementBounds box = unturnedBounds(placement, asset);
  const float narrower = std::min(box.max.x - box.min.x, box.max.y - box.min.y);
  return {.at = {placement.position.x + 0.5f, placement.position.y + 0.5f,
                 placement.position.z},
          .yaw_degrees = editorActorYawDegrees(placement),
          .behavior = editorBehaviorIdOf(placement.behavior),
          .faction = placement.faction,
          .radius = std::clamp(narrower * 0.5f, EDITOR_ACTOR_MIN_RADIUS,
                               EDITOR_ACTOR_MAX_RADIUS),
          .height = std::max(box.max.z - box.min.z, EDITOR_ACTOR_MIN_RADIUS)};
}

EditorPlacement editorActorPose(const EditorPlacement& placement, Vec3 feet,
                                Vec2 facing) {
  EditorPlacement posed = placement;
  posed.position = {feet.x - 0.5f, feet.y - 0.5f, feet.z};
  if (facing.x != 0.0f || facing.y != 0.0f) {
    posed.rotation.z = std::atan2(facing.y, facing.x) * DEGREES_PER_RADIAN -
                       EDITOR_MODEL_FRONT_DEGREES;
  }
  return posed;
}

Vec2 editorActorFacing(const EditorPlacement& placement) {
  const float radians = editorActorYawDegrees(placement) / DEGREES_PER_RADIAN;
  return {std::cos(radians), std::sin(radians)};
}

}  // namespace eng::editor
