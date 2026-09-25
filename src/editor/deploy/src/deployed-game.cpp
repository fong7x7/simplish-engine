#include <algorithm>
#include <charconv>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
#include <editor/deploy/deployed-game.h>
#include <editor/project/project-text-file.h>
#include <editor/shell/editor-behavior-table.h>
#include <editor/shell/editor-character-table.h>
#include <editor/shell/editor-enemy-table.h>
#include <engine/sim/simulation.h>
#include <game/logic/game-logic-instance.h>
#include <game/world/game-world.h>
#include <game/world/stand-in-input.h>
#include <string>

namespace eng::editor {

namespace {

  /// Room in @p setup for @p logic to spawn into, as a playtest gives it,
  /// when there is logic.
  void makeRoomFor(game::GameSetup& setup,
                   const game::GameLogicInstance& logic) {
    if (logic.get() != nullptr) {
      setup.actor_capacity =
          std::max(setup.actor_capacity, game::GAME_LOGIC_ACTOR_CAPACITY);
    }
  }

  /// Players @p options seats: 1 to `sim::MAX_PLAYERS`.
  uint8_t seatsFor(const DeployedGameOptions& options) {
    return static_cast<uint8_t>(
        std::clamp<unsigned>(options.players, 1, sim::MAX_PLAYERS));
  }

  /// Every seated player's input, each played by a stand-in.
  sim::TickInput standInsFor(const game::GameWorld& world, uint8_t players) {
    sim::TickInput input;
    for (uint8_t slot = 0; slot < players; ++slot) {
      input.players[slot] = game::standInInput(world, slot);
    }
    return input;
  }

  /// Keep what @p result says of its tick in @p run: its hash, and — when
  /// @p hashes asks — the whole of it.
  void keepTick(const sim::TickResult& result, DeployedHashes hashes,
                DeployedGameRun& run) {
    run.hash = result.hash ? result.hash->combined : 0;
    if (hashes == DeployedHashes::EVERY_TICK && result.hash) {
      run.tick_hashes.push_back(*result.hash);
    }
  }

  /// Step @p world until its run is over or @p run's ticks are spent,
  /// saying what its logic says to @p out.
  void play(game::GameWorld& world, const DeployedGameOptions& options,
            DeployedGameRun& run, std::ostream& out) {
    sim::Simulation simulation(world, sim::TickHashing::ON);
    const uint8_t players = seatsFor(options);
    while (!world.runOver() && simulation.nextTick() < options.max_ticks) {
      const sim::TickResult result =
          simulation.step(standInsFor(world, players));
      keepTick(result, options.hashes, run);
      for (const std::string& line : world.takeLogicLog()) {
        out << "[logic " << result.tick << "] " << line << '\n';
      }
    }
    run.ticks = simulation.nextTick();
    run.outcome = world.outcome();
  }

  /// The setup of the level @p options asks for — the manifest's start
  /// level when it asks for none — naming it in @p run; nothing, with
  /// @p run's error saying why, when there is none.
  std::optional<game::GameSetup>
  manifestSetup(const DeployedGameOptions& options, DeployedGameRun& run) {
    const std::optional<std::string> text =
        readProjectTextFile(options.content / EDITOR_DEPLOY_MANIFEST);
    const auto manifest = text ? parseDeployManifest(*text) : std::nullopt;
    run.level = options.level.empty() && manifest ? manifest->start_level
                                                  : options.level;
    auto setup =
        manifest ? readDeployedSetup(options.content, run.level) : std::nullopt;
    if (!setup) {
      run.error = manifest ? "No level " + run.level + " in this game"
                           : "No deployed game at " + options.content.string();
    }
    return setup;
  }

  /// @p text as a whole number, or nothing.
  std::optional<uint64_t> wholeNumber(std::string_view text) {
    uint64_t value = 0;
    const auto [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && end == text.data() + text.size()
               ? std::optional{value}
               : std::nullopt;
  }

  /// Take the flag @p flag's value @p value into @p options. False when
  /// the flag is not one, or its value does not fit it.
  bool applyFlag(DeployedGameOptions& options, std::string_view flag,
                 std::string_view value) {
    const std::optional<uint64_t> number = wholeNumber(value);
    if (flag == "--content") {
      options.content = std::filesystem::path(std::string(value));
    } else if (flag == "--level") {
      options.level = std::string(value);
    } else if (flag == "--ticks" && number) {
      options.max_ticks = *number;
    } else if (flag == "--players" && number && *number >= 1 &&
               *number <= sim::MAX_PLAYERS) {
      options.players = static_cast<uint8_t>(*number);
    } else {
      return false;
    }
    return true;
  }

}  // namespace

DeployedGameRun runDeployedGame(const DeployedGameOptions& options,
                                game::GameLogicFactory logic,
                                std::ostream& out) {
  DeployedGameRun run;
  std::optional<game::GameSetup> setup = manifestSetup(options, run);
  if (!setup) {
    return run;
  }
  // A deploy bakes every seat; the run seats as many as it was asked for.
  setup->player_count = seatsFor(options);
  run.players = setup->player_count;
  const game::GameLogicInstance instance(logic);
  makeRoomFor(*setup, instance);
  game::GameWorld world(*setup, readDeployedContent(options.content),
                        instance.get());
  run.logic = world.hasLogic();
  play(world, options, run, out);
  return run;
}

std::optional<DeployedGameOptions>
parseDeployedGameArgs(std::span<const std::string_view> args) {
  DeployedGameOptions options;
  for (size_t i = 0; i < args.size(); i += 2) {
    if (i + 1 >= args.size() || !applyFlag(options, args[i], args[i + 1])) {
      return std::nullopt;
    }
  }
  return options;
}

}  // namespace eng::editor
