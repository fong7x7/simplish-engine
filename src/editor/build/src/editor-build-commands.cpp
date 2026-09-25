#include <editor/build/editor-build-commands.h>
#include <editor/build/editor-build-paths.h>
#include <editor/project/project-paths.h>

namespace eng::editor {

namespace {

  /// `cmake -S source -B build -G generator`, and the tools @p tools
  /// names, as the start of a configure command.
  EditorBuildCommand configure(const std::filesystem::path& source,
                               const std::filesystem::path& build,
                               const EditorToolchain& tools) {
    EditorBuildCommand command{{tools.cmake, "-S", source.string(), "-B",
                                build.string(), "-G", tools.generator}};
    if (!tools.make_program.empty()) {
      command.words.push_back("-DCMAKE_MAKE_PROGRAM=" + tools.make_program);
    }
    if (!tools.cxx_compiler.empty()) {
      command.words.push_back("-DCMAKE_CXX_COMPILER=" + tools.cxx_compiler);
    }
    return command;
  }

  /// @p word in single quotes for a POSIX shell, or double quotes for
  /// Windows' `cmd`.
  std::string quoted(const std::string& word) {
#ifdef _WIN32
    return "\"" + word + "\"";
#else
    std::string out = "'";
    for (const char c : word) {
      out += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return out + "'";
#endif
  }

}  // namespace

std::vector<EditorBuildCommand>
logicBuildCommands(const std::filesystem::path& root,
                   const EditorToolchain& tools) {
  const std::filesystem::path build = projectLogicBuildPath(root);
  EditorBuildCommand setup = configure(projectSourcePath(root), build, tools);
  setup.words.emplace_back("-DCMAKE_BUILD_TYPE=Debug");
  setup.words.push_back("-DSIMPLISH_ROOT=" + tools.engine_root.string());
  // --config: a multi-config generator (Visual Studio, Xcode) ignores
  // CMAKE_BUILD_TYPE; a single-config one ignores this.
  return {setup,
          {{tools.cmake, "--build", build.string(), "--config", "Debug"}}};
}

std::vector<EditorBuildCommand>
deployBuildCommands(const std::filesystem::path& root,
                    const EditorToolchain& tools) {
  const std::filesystem::path build = projectDeployBuildPath(root);
  EditorBuildCommand setup = configure(tools.engine_root, build, tools);
  setup.words.emplace_back("-DCMAKE_BUILD_TYPE=Release");
  setup.words.emplace_back("-DENGINE_BUILD_TESTS=OFF");
  setup.words.push_back("-DSIMPLISH_PROJECT_DIR=" + root.string());
  return {setup,
          {{tools.cmake, "--build", build.string(), "--config", "Release",
            "--target", "simplish-game-app"}}};
}

std::string shellLine(const EditorBuildCommand& command,
                      const std::filesystem::path& log) {
  std::string line;
  for (const std::string& word : command.words) {
    line += (line.empty() ? "" : " ") + quoted(word);
  }
  line += " >> " + quoted(log.string()) + " 2>&1";
#ifdef _WIN32
  // cmd /c strips the first and last quote of a line that starts with one;
  // a pair around the whole line is what it strips.
  return "\"" + line + "\"";
#else
  return line;
#endif
}

}  // namespace eng::editor
