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

TEST_CASE("a clang error is taken apart into file, line, column and message") {
  const std::vector<std::string> lines = {
      "/p/src/game-logic.cpp:12:3: error: use of undeclared identifier 'x'",
      "/p/src/game-logic.cpp:12:3: error: use of undeclared identifier 'x'",
      "/p/src/game-logic.cpp:4:1: note: declared here",
      "/p/src/game-logic.cpp:20:9: warning: unused variable 'y'"};

  const auto found = buildDiagnostics(lines, 10);

  REQUIRE(found.size() == 2);
  CHECK(found[0].file == "/p/src/game-logic.cpp");
  CHECK(found[0].line == 12);
  CHECK(found[0].column == 3);
  CHECK(found[0].message == "use of undeclared identifier 'x'");
  CHECK(found[1].severity == EditorDiagnosticSeverity::WARNING);
}

TEST_CASE("MSVC, CMake, linker and the build's own errors are found too") {
  const std::vector<std::string> lines = {
      R"(C:\p\src\logic.cpp(7,15): error C2143: syntax error)",
      "CMake Error at CMakeLists.txt:14 (include):",
      "clang++: error: linker command failed with exit code 1",
      "error: the game logic would not load: no such file"};

  const auto found = buildDiagnostics(lines, 10);

  REQUIRE(found.size() == 4);
  CHECK(found[0].file == R"(C:\p\src\logic.cpp)");
  CHECK(found[0].line == 7);
  CHECK(found[0].column == 15);
  CHECK(found[1].file == "CMakeLists.txt");
  CHECK(found[1].line == 14);
  CHECK(found[2].file.empty());
  CHECK(found[2].message.starts_with("clang++: linker command failed"));
  CHECK(found[3].message == "the game logic would not load: no such file");
}
