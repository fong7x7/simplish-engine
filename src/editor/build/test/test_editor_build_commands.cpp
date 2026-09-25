#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-build-commands.h>

using namespace eng::editor;

namespace {

EditorToolchain tools() {
  return {"/usr/bin/cmake", "Ninja", "/usr/bin/ninja", "/usr/bin/clang++",
          "/src/simplish"};
}

bool has(const EditorBuildCommand& command, const std::string& word) {
  return std::find(command.words.begin(), command.words.end(), word) !=
         command.words.end();
}

}  // namespace

TEST_CASE("building logic configures the project's src against the engine") {
  const auto commands = logicBuildCommands("/games/dig", tools());

  REQUIRE(commands.size() == 2);
  REQUIRE(commands[0].words.front() == "/usr/bin/cmake");
  REQUIRE(has(commands[0], "/games/dig/src"));
  REQUIRE(has(commands[0], "-DSIMPLISH_ROOT=/src/simplish"));
  REQUIRE(has(commands[0], "-DCMAKE_CXX_COMPILER=/usr/bin/clang++"));
  REQUIRE(has(commands[1], "--build"));
}

TEST_CASE("deploying configures the engine with the project linked in") {
  const auto commands = deployBuildCommands("/games/dig", tools());

  REQUIRE(commands.size() == 2);
  REQUIRE(has(commands[0], "/src/simplish"));
  REQUIRE(has(commands[0], "-DSIMPLISH_PROJECT_DIR=/games/dig"));
  REQUIRE(has(commands[0], "-DCMAKE_BUILD_TYPE=Release"));
  REQUIRE(has(commands[1], "simplish-game-app"));
}

TEST_CASE("a shell line quotes every word and appends to its log") {
  const std::string line =
      shellLine({{"cmake", "-S", "/my games/it's"}}, "/tmp/b.log");

#ifndef _WIN32
  REQUIRE(line == "'cmake' '-S' '/my games/it'\\''s' >> '/tmp/b.log' 2>&1");
#else
  REQUIRE(line.find("\"/my games/it's\"") != std::string::npos);
#endif
}

TEST_CASE("both builds name their configuration, for multi-config "
          "generators") {
  const auto logic = logicBuildCommands("/games/dig", tools());
  const auto deploy = deployBuildCommands("/games/dig", tools());

  REQUIRE(has(logic[1], "--config"));
  REQUIRE(has(logic[1], "Debug"));
  REQUIRE(has(deploy[1], "Release"));
}
