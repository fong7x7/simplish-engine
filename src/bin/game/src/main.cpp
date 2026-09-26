#include <editor/deploy/deployed-game-options.h>
#include <editor/deploy/deployed-game.h>
#include <filesystem>
#include <game/logic/game-logic-entry.h>
#include <game/logic/run-outcome.h>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

// The project's game logic, linked in: its SIMPLISH_GAME_LOGIC, or
// no-game-logic.cpp's when the game was built without a project.
extern "C" eng::game::GameLogic* simplishCreateGameLogic();
extern "C" void simplishDestroyGameLogic(eng::game::GameLogic* logic);

namespace {

/// The deployed content beside the executable at @p argv0.
std::filesystem::path contentBeside(const char* argv0) {
  std::error_code ec;
  const std::filesystem::path exe = std::filesystem::absolute(argv0, ec);
  return exe.parent_path() / "game";
}

/// How the run ended, in a word.
std::string_view outcomeWord(eng::game::RunOutcome outcome) {
  if (outcome == eng::game::RunOutcome::WON) {
    return "won";
  }
  return outcome == eng::game::RunOutcome::LOST ? "lost" : "still playing";
}

/// Print how @p run ended to standard output — or why it could not run,
/// to standard error — and give the process's exit code.
int report(const eng::editor::DeployedGameRun& run) {
  if (!run.error.empty()) {
    std::cerr << run.error << '\n';
    return 1;
  }
  std::cout << run.level << ", " << static_cast<int>(run.players)
            << (run.players == 1 ? " player" : " players") << ": "
            << outcomeWord(run.outcome) << " after " << run.ticks << " ticks"
            << (run.logic ? "" : " (no game logic)") << "; hash " << std::hex
            << run.hash << '\n';
  return 0;
}

}  // namespace

/// Usage: simplish-game [--level ID] [--ticks N] [--players N]
///                      [--content DIR]
///                      [--serve PORT | --host PORT | --join HOST[:PORT]]
///                      [--delay TICKS|auto] [--pace real|fast]
///                      [--replay FILE] [--verify FILE] [--desync-dir DIR]
///
/// Runs a project's deployed game headless: every player a stand-in, until
/// the run is over or the ticks run out, then prints how it ended and the
/// last tick's hash. The content is the `game/` folder a deploy puts
/// beside this executable, unless `--content` says otherwise.
///
/// With `--serve` it is a dedicated co-op server, with `--host` a server
/// with a player of its own, and with `--join` a player in someone else's
/// session (ADR-013); `--players` is then how many a server waits for.
/// `--replay` records the run; `--verify` plays a recording back instead
/// and says whether it reproduces.
int main(int argc, char** argv) {  // NOLINT(bugprone-exception-escape)
  const std::vector<std::string_view> args(argv + 1, argv + argc);
  std::optional<eng::editor::DeployedGameOptions> options =
      eng::editor::parseDeployedGameArgs(args);
  if (!options) {
    std::cerr
        << "usage: simplish-game [--level ID] [--ticks N] "
           "[--players 1-4] [--content DIR]\n"
           "                     [--serve PORT | --host PORT | "
           "--join HOST[:PORT]]\n"
           "                     [--delay TICKS|auto] [--pace real|fast]\n"
           "                     [--replay FILE] [--verify FILE] "
           "[--desync-dir DIR]\n";
    return 2;
  }
  if (options->content.empty()) {
    options->content = contentBeside(argv[0]);
  }
  const eng::editor::DeployedGameRun run = eng::editor::runDeployedGame(
      *options, {simplishCreateGameLogic, simplishDestroyGameLogic}, std::cout);
  if (options->mode == eng::editor::DeployedGameMode::VERIFY &&
      run.error.empty()) {
    std::cout << "The replay reproduces its run\n";
  }
  return report(run);
}
