#include <catch2/catch_test_macros.hpp>
#include <game/ui/ui-text.h>

using eng::game::fillUiText;
using eng::game::uiNumber;
using eng::game::UiValues;

TEST_CASE("a screen's text shows its values, and nothing for one unset") {
  const UiValues values{{"score", "42"}, {"wave", "3"}};

  CHECK(fillUiText("Score: {score}", values) == "Score: 42");
  CHECK(fillUiText("Wave {wave} — {missing}!", values) == "Wave 3 — !");
  CHECK(fillUiText("{{braces}} and {score}", values) == "{braces}} and 42");
  CHECK(fillUiText("unclosed {score", values) == "unclosed {score");
}

TEST_CASE("a bar's number is a value's, or the key read as one") {
  const UiValues values{{"health", "7"}, {"name", "Ada"}};

  CHECK(uiNumber(values, "health") == 7.0F);
  CHECK(uiNumber(values, "10") == 10.0F);
  CHECK_FALSE(uiNumber(values, "name").has_value());
  CHECK_FALSE(uiNumber(values, "unset").has_value());
}
