#include "deployed-net-world.h"

#include <editor/deploy/deployed-content.h>
#include <game/world/stand-in-input.h>
#include <optional>
#include <utility>

namespace eng::editor {

namespace {

  /// The baked @p setup, made into the one @p start describes: its seed,
  /// its players, and the character each seat asked for.
  game::GameSetup startedAs(game::GameSetup setup, const net::NetStart& start) {
    setup.seed = start.header.seed;
    setup.player_count = start.header.player_count;
    for (uint8_t slot = 0; slot < setup.player_count; ++slot) {
      if (!start.header.characters[slot].empty()) {
        setup.characters[slot] = start.header.characters[slot];
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
                         const net::NetStart& start,
                         game::GameLogicFactory logic) {
  const std::optional<game::GameSetup> setup =
      readDeployedSetup(content, start.header.level_id);
  if (!setup) {
    return nullptr;
  }
  return std::make_unique<DeployedNetWorld>(
      startedAs(*setup, start), readDeployedContent(content), logic);
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
  return simulation_.step(input);
}

std::vector<std::string> DeployedNetWorld::takeLogicLog() {
  return world_.takeLogicLog();
}

}  // namespace eng::editor
