#include "deployed-level.h"
#include "deployed-net-world.h"
#include "deployed-replay.h"

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
#include <engine/net/net-input-delay.h>
#include <engine/sim/replay-recorder.h>
#include <engine/sim/simulation.h>
#include <game/logic/game-logic-instance.h>
#include <game/world/game-world.h>
#include <game/world/stand-in-input.h>
#include <memory>
#include <string>

namespace eng::editor {

namespace {

  /// Players @p options seats: 1 to `sim::MAX_PLAYERS`.
  uint8_t seatsFor(const DeployedGameOptions& options) {
    return static_cast<uint8_t>(
        std::clamp<unsigned>(options.players, 1, sim::MAX_PLAYERS));
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

  /// Step @p world until its run is over or @p run's ticks are spent —
  /// every seat absent, so every player a stand-in — saying what its logic
  /// says to @p out.
  void play(DeployedNetWorld& world, const DeployedGameOptions& options,
            DeployedGameRun& run, std::ostream& out) {
    const net::NetFrame nobody{
        0, 0, static_cast<uint8_t>((1U << seatsFor(options)) - 1U), {}};
    while (!world.world().runOver() && world.nextTick() < options.max_ticks) {
      const sim::TickResult result = world.step(nobody);
      keepTick(result, options.hashes, run);
      for (const std::string& line : world.takeLogicLog()) {
        out << "[logic " << result.tick << "] " << line << '\n';
      }
    }
    run.ticks = world.nextTick();
    run.outcome = world.world().outcome();
  }

  /// What a solo run of @p setup, as @p options seats it, starts from —
  /// with the content's hash when the run is to be recorded.
  sim::ReplayHeader soloHeader(const DeployedGameOptions& options,
                               const std::string& level,
                               const game::GameSetup& setup) {
    // A deploy bakes every seat; the run seats as many as it was asked for.
    sim::ReplayHeader header{level, 0, setup.seed, seatsFor(options), {}};
    header.content_hash =
        options.replay.empty() ? 0 : deployedContentHash(options.content);
    for (uint8_t slot = 0; slot < header.player_count; ++slot) {
      header.characters[slot] = setup.characters[slot];
    }
    return header;
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

  /// The input delay @p value names: `auto` to measure it at each start,
  /// or 1 to `net::NET_MAX_INPUT_DELAY` ticks.
  bool delayFlag(DeployedGameOptions& options, std::string_view value) {
    const std::optional<uint64_t> ticks = wholeNumber(value);
    if (value == "auto") {
      options.input_delay.reset();
      return true;
    }
    if (!ticks || *ticks < 1 || *ticks > net::NET_MAX_INPUT_DELAY) {
      return false;
    }
    options.input_delay = static_cast<uint8_t>(*ticks);
    return true;
  }

  /// How many whole seconds, at least 1, a run waits on a seat before
  /// dropping it: @p value.
  bool stallDropFlag(DeployedGameOptions& options, std::string_view value) {
    const std::optional<uint64_t> seconds = wholeNumber(value);
    if (!seconds || *seconds < 1 || *seconds > DEPLOYED_MAX_STALL_DROP_S) {
      return false;
    }
    options.stall_drop = std::chrono::seconds(*seconds);
    return true;
  }

  /// A path-valued flag @p flag, valued @p value, into @p options. False
  /// when it is not one.
  bool pathFlag(DeployedGameOptions& options, std::string_view flag,
                std::string_view value) {
    const std::filesystem::path path{std::string(value)};
    if (flag == "--replay") {
      options.replay = path;
    } else if (flag == "--verify") {
      options.mode = DeployedGameMode::VERIFY;
      options.verify = path;
    } else if (flag == "--desync-dir") {
      options.desync_dir = path;
    } else if (flag == "--password") {
      options.password = std::string(value);
    } else {
      return false;
    }
    return !value.empty();
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
    if (flag == "--stall-drop") {
      return stallDropFlag(options, value);
    }
    if (flag == "--pace") {
      return paceFlag(options, value);
    }
    return pathFlag(options, flag, value);
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
  /// The world of a solo run of @p setup as @p options seats it, with an
  /// instance of the logic @p logic makes, recording when asked; the run's
  /// players and logic said in @p run. Null when there is no such level.
  std::unique_ptr<DeployedNetWorld>
  soloWorld(const DeployedGameOptions& options, const game::GameSetup& setup,
            game::GameLogicFactory logic, DeployedGameRun& run) {
    const sim::ReplayHeader header = soloHeader(options, run.level, setup);
    auto world = DeployedNetWorld::create(options.content, header, logic);
    if (world) {
      run.players = header.player_count;
      run.logic = world->world().hasLogic();
    }
    if (world && !options.replay.empty()) {
      world->startRecording(header);
    }
    return world;
  }

  DeployedGameRun runSolo(const DeployedGameOptions& options,
                          game::GameLogicFactory logic, std::ostream& out) {
    DeployedGameRun run;
    const std::optional<game::GameSetup> setup = manifestSetup(options, run);
    auto world = setup ? soloWorld(options, *setup, logic, run) : nullptr;
    if (!world) {
      return run;
    }
    play(*world, options, run, out);
    if (const std::optional<sim::Replay> replay = world->replay()) {
      saveReplay(options, *replay, out);
    }
    return run;
  }

}  // namespace

DeployedGameRun runDeployedGame(const DeployedGameOptions& options,
                                game::GameLogicFactory logic,
                                std::ostream& out) {
  if (options.mode == DeployedGameMode::VERIFY) {
    return verifyDeployedReplay(options, logic);
  }
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
