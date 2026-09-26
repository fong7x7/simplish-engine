#include "deployed-replay.h"

#include "deployed-net-world.h"

#include <editor/deploy/deployed-content.h>
#include <engine/sim/replay-codec.h>
#include <engine/sim/replay-verification.h>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace eng::editor {

namespace {

  /// @p replay written to @p path. False when it could not be.
  bool writeReplayFile(const std::filesystem::path& path,
                       const sim::Replay& replay) {
    const std::vector<std::byte> bytes = sim::encodeReplay(replay);
    std::ofstream file(path, std::ios::binary);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) bytes are
    // chars
    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return file.good();
  }

  /// The run @p replay is, before it is played: level, players, and the
  /// hash its last checkpoint says it ended on.
  DeployedGameRun runOf(const sim::Replay& replay) {
    DeployedGameRun run;
    run.level = replay.header.level_id;
    run.players = replay.header.player_count;
    run.hash =
        replay.checkpoints.empty() ? 0 : replay.checkpoints.back().combined;
    return run;
  }

  /// Why @p replay cannot be played against @p options' content, if it
  /// cannot.
  std::string_view unplayable(const sim::Replay& replay,
                              const DeployedGameOptions& options) {
    if (replay.header.content_hash != deployedContentHash(options.content)) {
      return "The replay was recorded with other content";
    }
    return {};
  }

  /// @p verification, in words, into @p run.
  void judge(const sim::ReplayVerification& verification,
             DeployedGameRun& run) {
    run.ticks = verification.ticks_run;
    if (verification.divergence) {
      run.error = "The replay diverges at tick " +
                  std::to_string(verification.divergence->tick) +
                  " in section " +
                  std::string(verification.divergence->section);
    }
  }

  /// Play @p replay back in a world built from @p content, with the logic
  /// @p logic makes, judging it into @p run.
  void playBack(const sim::Replay& replay, const std::filesystem::path& content,
                game::GameLogicFactory logic, DeployedGameRun& run) {
    auto world = DeployedNetWorld::create(content, replay.header, logic);
    if (!world) {
      run.error = "No level " + run.level + " in this game";
      return;
    }
    judge(sim::verifyReplay(replay, world->simulation()), run);
    run.logic = world->world().hasLogic();
    run.outcome = world->world().outcome();
  }

}  // namespace

void saveReplay(const DeployedGameOptions& options, const sim::Replay& replay,
                std::ostream& out) {
  if (options.replay.empty()) {
    return;
  }
  out << (writeReplayFile(options.replay, replay) ? "Replay written to "
                                                  : "Could not write the "
                                                    "replay to ")
      << options.replay.string() << '\n';
}

std::optional<sim::Replay> readReplayFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  const std::vector<char> chars{std::istreambuf_iterator<char>(file),
                                std::istreambuf_iterator<char>()};
  auto decoded = sim::decodeReplay(std::as_bytes(std::span(chars)));
  return decoded ? std::optional{std::move(*decoded)} : std::nullopt;
}

DeployedGameRun verifyDeployedReplay(const DeployedGameOptions& options,
                                     game::GameLogicFactory logic) {
  const std::optional<sim::Replay> replay = readReplayFile(options.verify);
  if (!replay) {
    return {.error = "No replay at " + options.verify.string()};
  }
  DeployedGameRun run = runOf(*replay);
  run.error = std::string(unplayable(*replay, options));
  if (run.error.empty()) {
    playBack(*replay, options.content, logic, run);
  }
  return run;
}

}  // namespace eng::editor
