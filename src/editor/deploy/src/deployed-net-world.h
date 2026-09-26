#pragma once

/// @file deployed-net-world.h
/// @brief One networked run of a deployed game: its world and simulation.
/// @par Threading Main-thread-only.

#include <engine/net/net-frame.h>
#include <engine/net/net-start.h>
#include <engine/sim/replay-header.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/replay.h>
#include <engine/sim/simulation.h>
#include <engine/sim/tick-result.h>
#include <filesystem>
#include <game/content/game-content.h>
#include <game/logic/game-logic-factory.h>
#include <game/logic/game-logic-instance.h>
#include <game/world/game-setup.h>
#include <game/world/game-world.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eng::editor {

/// The world a run of a co-op session plays in, built from the run's start
/// exactly as every other peer builds it, and stepped only on frames.
/// @thread_safety Main-thread-only.
class DeployedNetWorld {
public:
  /// The world of the run @p header describes — a co-op run's start, or a
  /// replay's — from the deployed content at @p content, with an instance
  /// of the logic @p logic makes. Null when the content has no such level.
  [[nodiscard]] static std::unique_ptr<DeployedNetWorld>
  create(const std::filesystem::path& content, const sim::ReplayHeader& header,
         game::GameLogicFactory logic);

  /// A world set up as @p setup, played with @p content and @p logic.
  DeployedNetWorld(const game::GameSetup& setup, game::GameContent content,
                   game::GameLogicFactory logic);

  /// Step one tick on @p frame, its absent seats played by stand-ins — and
  /// record the input stepped, when recording.
  sim::TickResult step(const net::NetFrame& frame);

  /// Record every step from now on, as the run @p header describes.
  void startRecording(const sim::ReplayHeader& header);

  /// The run so far as a replay, when recording.
  [[nodiscard]] std::optional<sim::Replay> replay() const;

  /// The simulation, to step directly — as a replay's playback does.
  [[nodiscard]] sim::Simulation& simulation() { return simulation_; }

  /// The world.
  [[nodiscard]] const game::GameWorld& world() const { return world_; }
  /// The tick the next step simulates.
  [[nodiscard]] uint64_t nextTick() const { return simulation_.nextTick(); }
  /// What the logic has said since the last call.
  [[nodiscard]] std::vector<std::string> takeLogicLog();

private:
  /// The data tables the world reads.
  game::GameContent content_;
  /// The project's logic for this run, if it has any.
  game::GameLogicInstance logic_;
  /// The world.
  game::GameWorld world_;
  /// The tick over it, hashing every tick.
  sim::Simulation simulation_;
  /// The run's replay, while recording.
  std::optional<sim::ReplayRecorder> recorder_;
};

}  // namespace eng::editor
