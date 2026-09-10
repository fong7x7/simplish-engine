#pragma once

/// @file editor-playtest-session.h
/// @brief The game running inside the editor, from Play until Stop.
/// @par Threading Main-thread-only.

#include <cstddef>
#include <cstdint>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-document.h>
#include <editor/shell/editor-playtest-state.h>
#include <editor/shell/editor-scripted-input.h>
#include <editor/shell/iso-projection.h>
#include <engine/core/fixed-step-advance.h>
#include <engine/core/fixed-step-clock.h>
#include <engine/math/vec3.h>
#include <engine/sim/player-input.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/replay.h>
#include <engine/sim/simulation.h>
#include <engine/sim/tick-hash.h>
#include <filesystem>
#include <game/player/player-pool.h>
#include <game/world/game-setup.h>
#include <game/world/game-world.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

/// The seed every playtest runs with. Fixed, so two playtests of the same
/// level on the same inputs are the same run — which is what makes one
/// driven by the agent API's `send_input` a test rather than a demo.
inline constexpr uint64_t EDITOR_PLAYTEST_SEED = 0;

/// What a playtest of @p document starts from: one player, standing on the
/// first start for player 1 in the document, or on @p fallback — the tile
/// under the camera — when it has none (Editor REQUIREMENTS §7: "starting at
/// the camera position or a chosen spawn point"); and a collision box for
/// every placement that collides, measured against @p assets.
///
/// The box is the one the viewport outlines and picks: the placement's
/// asset, turned and set where it stands, enclosed in an axis-aligned box.
/// A prop turned 45° therefore blocks a little more than its mesh covers;
/// that is the price of boxes, and what a collision shape authored per
/// asset would fix.
[[nodiscard]] game::GameSetup
makeEditorPlaytestSetup(const EditorDocument& document,
                        const std::vector<EditorAsset>& assets,
                        WorldPoint fallback);

/// Where the replay of the last playtest of @p level_id is kept:
/// `<root>/data/playtests/<level_id>.replay`. Editor scratch rather than
/// content ([project-format.md §11]): overwritten by the next playtest of
/// that level, and never generated or shipped.
[[nodiscard]] std::filesystem::path
editorPlaytestReplayPath(const std::filesystem::path& root,
                         std::string_view level_id);

/// Write @p replay to `editorPlaytestReplayPath`, making the directory if
/// it is not there. False when it could not be written.
bool writeEditorPlaytestReplay(const std::filesystem::path& root,
                               std::string_view level_id,
                               const sim::Replay& replay);

/// One playtest: the game's world, the simulation stepping it, the clock
/// pacing it, and the replay recording it.
///
/// Built from a copy of the level when Play is pressed and thrown away when
/// it is stopped, so a playtest cannot change the document and nothing
/// needs restoring afterwards. The editor owns one while playing and none
/// otherwise; everything about the running game goes through here, which
/// is what keeps it testable with no window, no GPU and no clock.
/// @thread_safety Main-thread-only.
class EditorPlaytestSession {
public:
  /// A playtest at tick 0 of the level @p level_id, set up as @p setup.
  EditorPlaytestSession(const game::GameSetup& setup,
                        const std::string& level_id);

  /// Runs every tick @p elapsed_ns of real time pays for, at most four,
  /// each on player 1's next input: the front of @p scripted while any is
  /// queued, @p live otherwise.
  FixedStepAdvance advance(uint64_t elapsed_ns, const sim::PlayerInput& live,
                           std::vector<EditorScriptedInput>& scripted);

  /// Runs exactly one tick, as `advance` does for each tick it runs.
  void step(const sim::PlayerInput& live,
            std::vector<EditorScriptedInput>& scripted);

  /// Copies what the rest of the editor may know — tick, hash, where the
  /// players are — into @p state.
  void publish(EditorPlaytestState& state) const;

  /// Where the player at dense index @p index is drawn, @p alpha of the way
  /// from where the last tick found it to where it left it. Render-side
  /// only: the simulation never sees it.
  [[nodiscard]] Vec3 renderPosition(size_t index, float alpha) const;

  /// The players, as the simulation holds them.
  [[nodiscard]] const game::PlayerPool& players() const;

  /// Ticks simulated so far.
  [[nodiscard]] uint64_t tick() const { return simulation_.nextTick(); }

  /// The run so far, as a replay (Editor REQUIREMENTS §7: "every playtest
  /// records a replay").
  [[nodiscard]] sim::Replay replay() const { return recorder_.finish(); }

private:
  /// The game being played. Held by pointer because `simulation_` holds
  /// its address, and the session must be movable without moving it.
  std::unique_ptr<game::GameWorld> world_;
  /// Steps `world_`, hashing every tick.
  sim::Simulation simulation_;
  /// Turns frame time into ticks.
  FixedStepClock clock_;
  /// Every tick's input and a checkpoint a second.
  sim::ReplayRecorder recorder_;
  /// Each player's position before the last tick, for interpolation.
  std::vector<Vec3> previous_;
  /// Ticks the clock has dropped, summed over the playtest.
  uint64_t dropped_ticks_ = 0;
  /// The hash the last tick ended on.
  std::optional<sim::TickHash> last_hash_;
};

}  // namespace eng::editor
