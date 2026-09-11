#include <catch2/catch_test_macros.hpp>
#include <game/content/character-lookup.h>

using namespace eng::game;

namespace {

/// Content with two characters.
GameContent twoCharacters() {
  GameContent content;
  content.characters.push_back({"scout", "Scout", "", 7.5F, 3});
  content.characters.push_back({"tank", "Tank", "", 3.0F, 9});
  return content;
}

}  // namespace

TEST_CASE("a character is found by its id") {
  const GameContent content = twoCharacters();
  REQUIRE(resolveCharacter(content, "tank").health == 9);
  REQUIRE(resolveCharacter(content, "scout").move_speed == 7.5F);
}

TEST_CASE("no pick, or one the content lacks, is the default character") {
  const GameContent content = twoCharacters();
  REQUIRE(&resolveCharacter(content, "") == &defaultCharacter());
  REQUIRE(&resolveCharacter(content, "ghost") == &defaultCharacter());
  REQUIRE(defaultCharacter().move_speed == DEFAULT_CHARACTER_MOVE_SPEED);
  REQUIRE(defaultCharacter().health == DEFAULT_CHARACTER_HEALTH);
  REQUIRE(defaultCharacter().model.empty());
}

TEST_CASE("the default character moves what every player used to") {
  REQUIRE(characterSpeedPerTick(defaultCharacter()) == 5.0F / 60.0F);
  REQUIRE(characterSpeedPerTick({"", "", "", 6.0F, 1}) == 0.1F);
}
