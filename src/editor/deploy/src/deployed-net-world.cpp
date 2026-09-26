#include "deployed-net-world.h"

#include <editor/deploy/deployed-content.h>
#include <game/world/stand-in-input.h>
#include <optional>
#include <utility>

namespace eng::editor {

namespace {

  /// The baked @p setup, made into the one @p header describes: its seed,
  /// its players, and the character each seat asked for.
  game::GameSetup startedAs(game::GameSetup setup,
                            const sim::ReplayHeader& header) {
    setup.seed = header.seed;
    setup.player_count = header.player_count;
    for (uint8_t slot = 0; slot < setup.player_count; ++slot) {
      if (!header.characters[slot].empty()) {
        setup.characters[slot] = header.characters[slot];
      }
    }
    return setup;
  }

  /// @p setup with room for @p logic.
  game::GameSetup roomFor(game::GameSetup setup,
                          const game::GameLogicInstance& logic) {
    makeRoomForLogic(setup, logic);
    return setup;
  }

}  // namespace

std::unique_ptr<DeployedNetWorld>
DeployedNetWorld::create(const std::filesystem::path& content,
                         const sim::ReplayHeader& header,
                         game::GameLogicFactory logic) {
  const std::optional<game::GameSetup> setup =
      readDeployedSetup(content, header.level_id);
  if (!setup) {
    return nullptr;
  }
  return std::make_unique<DeployedNetWorld>(
      startedAs(*setup, header), readDeployedContent(content), logic);
}

DeployedNetWorld::DeployedNetWorld(const game::GameSetup& setup,
                                   game::GameContent content,
                                   game::GameLogicFactory logic)
  : content_(std::move(content)), logic_(logic),
    world_(roomFor(setup, logic_), content_, logic_.get()),
    simulation_(world_, sim::TickHashing::ON) {}

sim::TickResult DeployedNetWorld::step(const net::NetFrame& frame) {
  sim::TickInput input = frame.input;
  game::standInForAbsent(world_, frame.absent, input);
  sim::TickResult result = simulation_.step(input);
  if (recorder_) {
    recorder_->record(input, result);
  }
  return result;
}

void DeployedNetWorld::startRecording(const sim::ReplayHeader& header) {
  recorder_.emplace(header, sim::DEFAULT_CHECKPOINT_INTERVAL);
}

std::optional<sim::Replay> DeployedNetWorld::replay() const {
  return recorder_ ? std::optional{recorder_->finish()} : std::nullopt;
}

std::vector<std::string> DeployedNetWorld::takeLogicLog() {
  return world_.takeLogicLog();
}

}  // namespace eng::editor
