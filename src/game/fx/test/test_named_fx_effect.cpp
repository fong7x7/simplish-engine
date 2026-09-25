#include <catch2/catch_test_macros.hpp>
#include <game/fx/named-fx-effect.h>

using eng::game::findNamedFxEffect;

TEST_CASE("an effect is found by its combat cue's name, whole") {
  const auto blast = findNamedFxEffect("combat.blast");

  REQUIRE(blast.has_value());
  CHECK(blast->bursts.size() > 1);
  CHECK_FALSE(blast->volumes.empty());
}

TEST_CASE("an effect is found by a preset's id, as that one burst") {
  const auto smoke = findNamedFxEffect("smoke");

  REQUIRE(smoke.has_value());
  CHECK(smoke->bursts.size() == 1);
}

TEST_CASE("a name that is no effect finds nothing") {
  CHECK_FALSE(findNamedFxEffect("confetti").has_value());
  CHECK_FALSE(findNamedFxEffect("").has_value());
}
