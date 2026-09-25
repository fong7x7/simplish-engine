#include "support/build-temp-dir.h"
#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-build-log.h>
#include <fstream>

using namespace eng::editor;

TEST_CASE("a log's tail is its last lines, in order") {
  const test::BuildTempDir dir("log-tail");
  const auto log = dir.path() / "b.log";
  std::ofstream(log) << "one\ntwo\nthree\n";

  REQUIRE(readLogTail(log, 2) == std::vector<std::string>{"two", "three"});
  REQUIRE(readLogTail(log, 9).size() == 3);
  REQUIRE(readLogTail(dir.path() / "none.log", 2).empty());
}

TEST_CASE("the lines naming an error are picked out of a build log") {
  const std::vector<std::string> lines = {
      "[1/2] Building CXX object game-logic.cpp.o",
      "/usr/bin/c++ -Werror -Wconversion -c game-logic.cpp",
      "game-logic.cpp:12:3: error: use of undeclared identifier 'wrold'",
      "CMake Error at CMakeLists.txt:4 (include):",
      "ninja: build stopped: subcommand failed."};

  const auto errors = buildErrorLines(lines, 5);

  REQUIRE(errors.size() == 2);
  REQUIRE(buildErrorLines(std::vector<std::string>{"a.cpp(3): error C2143"},
                          5)
              .size() == 1);
  REQUIRE(errors[0].find("wrold") != std::string::npos);
  REQUIRE(buildErrorLines(lines, 1).size() == 1);
}
