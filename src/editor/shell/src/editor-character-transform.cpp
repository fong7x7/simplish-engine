#include <cmath>
#include <editor/shell/editor-character-transform.h>
#include <game/player/player-system.h>

namespace eng::editor {

namespace {

  /// The turn, in radians, that brings a model's front — world -Y — round
  /// to @p aim.
  float yawToward(Vec2 aim) {
    constexpr float QUARTER_TURN = 1.57079632679489661923f;
    if (aim.x == 0.0f && aim.y == 0.0f) {
      return QUARTER_TURN;
    }
    return std::atan2(aim.y, aim.x) + QUARTER_TURN;
  }

  /// Scale that makes @p asset as tall as a player.
  float heightScale(const EditorAsset& asset) {
    const float height = asset.max.z - asset.min.z;
    return height > 0.0f ? game::PLAYER_HEIGHT_TILES / height : 1.0f;
  }

}  // namespace

Mat4 makeEditorCharacterTransform(const EditorAsset& asset, Vec3 feet,
                                  Vec2 aim) {
  const float scale = heightScale(asset);
  const float yaw = yawToward(aim);
  const float c = std::cos(yaw) * scale;
  const float s = std::sin(yaw) * scale;
  const Vec3 pivot{(asset.min.x + asset.max.x) * 0.5f,
                   (asset.min.y + asset.max.y) * 0.5f, asset.min.z};
  Mat4 out = Mat4::identity();
  out(0, 0) = c;
  out(0, 1) = -s;
  out(1, 0) = s;
  out(1, 1) = c;
  out(2, 2) = scale;
  // The pivot, turned and scaled, lands on the feet.
  out(0, 3) = feet.x - (c * pivot.x - s * pivot.y);
  out(1, 3) = feet.y - (s * pivot.x + c * pivot.y);
  out(2, 3) = feet.z - scale * pivot.z;
  return out;
}

}  // namespace eng::editor
