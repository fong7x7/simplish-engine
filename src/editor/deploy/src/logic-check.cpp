#include <algorithm>
#include <charconv>
#include <editor/build/editor-logic-library-load.h>
#include <editor/deploy/deployed-game.h>
#include <editor/deploy/logic-check.h>
#include <engine/sim/hash-divergence.h>
#include <sstream>
#include <string>
#include <string_view>

namespace eng::editor {

namespace {

  /// Take the flag @p flag's value @p value into @p options. False when
  /// the flag is not one, or its value does not fit it.
  bool applyFlag(LogicCheckOptions& options, std::string_view flag,
                 std::string_view value) {
    if (flag == "--library") {
      options.library = std::filesystem::path(std::string(value));
    } else if (flag == "--content") {
      options.content = std::filesystem::path(std::string(value));
    } else if (flag == "--ticks") {
      const auto [end, ec] = std::from_chars(
          value.data(), value.data() + value.size(), options.ticks);
      return ec == std::errc{} && end == value.data() + value.size();
    } else {
      return false;
    }
    return true;
  }

  /// Where the check loads its copy of the library: beside the content, so
  /// the build can write the next library while this one is open.
  std::filesystem::path checkCopy(const LogicCheckOptions& options) {
    return options.content /
           ("game-logic-check" + options.library.extension().string());
  }

  /// A run of @p options' content with @p library's logic, keeping every
  /// tick's hash, saying what the logic says to @p out.
  DeployedGameRun checkRun(const LogicCheckOptions& options,
                           const EditorLogicLibrary& library,
                           std::ostream& out) {
    return runDeployedGame(
        {options.content, {}, options.ticks, 1, DeployedHashes::EVERY_TICK},
        library.factory(), out);
  }

  /// What diverging in @p section says about the logic.
  std::string_view divergenceCause(std::string_view section) {
    return section == "logic"
               ? "its own state differs: a clock, std::rand or <random>, an "
                 "unordered container iterated, uninitialised memory, or a "
                 "static that outlives a run"
               : "the world differs, so the logic changed it differently: "
                 "look for what it decides by that it does not hash, or "
                 "anything above";
  }

  /// Whether @p second ran exactly as @p first did; when not, say where it
  /// first differed to @p out.
  bool sameRun(const DeployedGameRun& first, const DeployedGameRun& second,
               std::ostream& out) {
    const size_t ticks =
        std::min(first.tick_hashes.size(), second.tick_hashes.size());
    for (size_t i = 0; i < ticks; ++i) {
      if (const auto diverged = sim::findDivergence(first.tick_hashes[i],
                                                    second.tick_hashes[i])) {
        out << "error: the game logic is not deterministic: a second run "
               "of the same level first differs on tick "
            << diverged->tick << ", in " << diverged->section << " — "
            << divergenceCause(diverged->section) << '\n';
        return false;
      }
    }
    return true;
  }

}  // namespace

int runLogicCheck(const LogicCheckOptions& options, std::ostream& out) {
  const EditorLogicLibraryLoad load =
      loadEditorLogicLibrary(options.library, checkCopy(options));
  if (load.library == nullptr) {
    out << "error: the game logic would not load: " << load.error << '\n';
    return 1;
  }
  out << "check: running the game logic for " << options.ticks
      << " ticks, twice\n";
  const DeployedGameRun first = checkRun(options, *load.library, out);
  if (!first.error.empty()) {
    out << "error: " << first.error << '\n';
    return 1;
  }
  std::ostringstream quiet;
  if (!sameRun(first, checkRun(options, *load.library, quiet), out)) {
    return 1;
  }
  out << "check: passed — " << first.ticks
      << " ticks without a crash, the same both times\n";
  return 0;
}

std::optional<LogicCheckOptions>
parseLogicCheckArgs(std::span<const std::string_view> args) {
  LogicCheckOptions options;
  for (size_t i = 0; i < args.size(); i += 2) {
    if (i + 1 >= args.size() || !applyFlag(options, args[i], args[i + 1])) {
      return std::nullopt;
    }
  }
  if (options.library.empty() || options.content.empty()) {
    return std::nullopt;
  }
  return options;
}

}  // namespace eng::editor
