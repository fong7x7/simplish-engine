#include "deployed-level.h"

#include <algorithm>
#include <charconv>
#include <editor/build/editor-deploy-manifest.h>
#include <editor/build/editor-setup-json.h>
#include <editor/deploy/deployed-content.h>
#include <editor/deploy/deployed-game.h>
#include <editor/deploy/deployed-session.h>
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

  /// @p text as a whole number, or nothing.
  std::optional<uint64_t> wholeNumber(std::string_view text) {
    uint64_t value = 0;
    const auto [end, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return ec == std::errc{} && end == text.data() + text.size()
               ? std::optional{value}
               : std::nullopt;
  }

  /// A UDP port from @p text, or nothing.
  std::optional<uint16_t> portNumber(std::string_view text) {
    const std::optional<uint64_t> number = wholeNumber(text);
    return number && *number <= UINT16_MAX
               ? std::optional{static_cast<uint16_t>(*number)}
               : std::nullopt;
  }

  /// Join the server @p where names — a host, and a port after a colon
  /// when it is not the default — in @p options. False when the port is
  /// not one.
  bool joinFlag(DeployedGameOptions& options, std::string_view where) {
    options.mode = DeployedGameMode::JOIN;
    const size_t colon = where.rfind(':');
    options.address = std::string(where.substr(0, colon));
    if (colon == std::string_view::npos) {
      return !options.address.empty();
    }
    const std::optional<uint16_t> port = portNumber(where.substr(colon + 1));
    options.port = port.value_or(0);
    return port.has_value() && !options.address.empty();
  }

  /// Serve or host, by @p flag, on the port @p value names.
  bool serveFlag(DeployedGameOptions& options, std::string_view flag,
                 std::string_view value) {
    const std::optional<uint16_t> port = portNumber(value);
    options.mode =
        flag == "--serve" ? DeployedGameMode::SERVE : DeployedGameMode::HOST;
    options.port = port.value_or(0);
    return port.has_value();
  }

  /// The input delay @p value names, 1 to `DEPLOYED_MAX_INPUT_DELAY`.
  bool delayFlag(DeployedGameOptions& options, std::string_view value) {
    const std::optional<uint64_t> ticks = wholeNumber(value);
    if (!ticks || *ticks < 1 || *ticks > DEPLOYED_MAX_INPUT_DELAY) {
      return false;
    }
    options.input_delay = static_cast<uint8_t>(*ticks);
    return true;
  }

  /// The pace @p value names: `real` or `fast`.
  bool paceFlag(DeployedGameOptions& options, std::string_view value) {
    options.pace =
        value == "fast" ? DeployedPace::FAST : DeployedPace::REAL_TIME;
    return value == "real" || value == "fast";
  }

  /// Take a co-op session's flag @p flag, valued @p value, into
  /// @p options (ADR-013). False when it is not one, or its value does
  /// not fit it.
  bool applySessionFlag(DeployedGameOptions& options, std::string_view flag,
                        std::string_view value) {
    if (flag == "--serve" || flag == "--host") {
      return serveFlag(options, flag, value);
    }
    if (flag == "--join") {
      return joinFlag(options, value);
    }
    if (flag == "--delay") {
      return delayFlag(options, value);
    }
    return flag == "--pace" && paceFlag(options, value);
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
      return applySessionFlag(options, flag, value);
    }
    return true;
  }

  /// The game @p options describes, alone: every seat a stand-in.
  DeployedGameRun runSolo(const DeployedGameOptions& options,
                          game::GameLogicFactory logic, std::ostream& out) {
    DeployedGameRun run;
    std::optional<game::GameSetup> setup = manifestSetup(options, run);
    if (!setup) {
      return run;
    }
    // A deploy bakes every seat; the run seats as many as it was asked for.
    setup->player_count = seatsFor(options);
    run.players = setup->player_count;
    const game::GameLogicInstance instance(logic);
    makeRoomForLogic(*setup, instance);
    game::GameWorld world(*setup, readDeployedContent(options.content),
                          instance.get());
    run.logic = world.hasLogic();
    play(world, options, run, out);
    return run;
  }

}  // namespace

DeployedGameRun runDeployedGame(const DeployedGameOptions& options,
                                game::GameLogicFactory logic,
                                std::ostream& out) {
  return options.mode == DeployedGameMode::SOLO
             ? runSolo(options, logic, out)
             : runDeployedSession(options, logic, out);
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
