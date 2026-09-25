#include <catch2/catch_test_macros.hpp>
#include <editor/deploy/logic-check.h>
#include <sstream>

using namespace eng::editor;

TEST_CASE("the logic check's arguments name a library and content") {
  const std::string_view args[] = {"--library", "/g/game-logic.dylib",
                                   "--content", "/g/check",
                                   "--ticks",   "60"};
  const std::string_view no_content[] = {"--library", "/g/game-logic.dylib"};

  const auto options = parseLogicCheckArgs(args);

  REQUIRE(options.has_value());
  CHECK(options->library == "/g/game-logic.dylib");
  CHECK(options->content == "/g/check");
  CHECK(options->ticks == 60);
  CHECK_FALSE(parseLogicCheckArgs(no_content).has_value());
}

TEST_CASE("the logic check fails, saying why, on a library that is not "
          "there") {
  std::ostringstream out;

  const int result =
      runLogicCheck({"/nowhere/game-logic.dylib", "/nowhere", 60}, out);

  CHECK(result == 1);
  CHECK(out.str().starts_with("error: the game logic would not load"));
}
