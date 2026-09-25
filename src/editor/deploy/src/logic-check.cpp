#include <charconv>
#include <editor/build/editor-logic-library-load.h>
#include <editor/deploy/deployed-game.h>
#include <editor/deploy/logic-check.h>
#include <string>

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

}  // namespace

int runLogicCheck(const LogicCheckOptions& options, std::ostream& out) {
  const EditorLogicLibraryLoad load =
      loadEditorLogicLibrary(options.library, checkCopy(options));
  if (load.library == nullptr) {
    out << "error: the game logic would not load: " << load.error << '\n';
    return 1;
  }
  out << "check: running the game logic for " << options.ticks << " ticks\n";
  const DeployedGameRun run = runDeployedGame(
      {options.content, {}, options.ticks, 1}, load.library->factory(), out);
  if (!run.error.empty()) {
    out << "error: " << run.error << '\n';
    return 1;
  }
  out << "check: passed — " << run.ticks << " ticks without a crash\n";
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
