#include <catch2/catch_test_macros.hpp>
#include <engine/animation/clip-event-crossing.h>
#include <vector>

using namespace eng::animation;

namespace {

/// The indices of @p times a one-second loop passes in @p window.
std::vector<size_t> crossing(std::vector<float> times, ClipWindow window) {
  std::vector<size_t> out;
  crossedClipTimes(times, 1.0F, window, out);
  return out;
}

}  // namespace

TEST_CASE("a frame passes the times between where it started and ended") {
  REQUIRE(crossing({0.25F, 0.75F}, {0.1, 0.3}) == std::vector<size_t>{0});
  REQUIRE(crossing({0.25F, 0.75F}, {0.3, 0.7}).empty());
}

TEST_CASE("a time on a frame's end counts; one on its start does not") {
  REQUIRE(crossing({0.5F}, {0.4, 0.5}) == std::vector<size_t>{0});
  REQUIRE(crossing({0.5F}, {0.5, 0.6}).empty());
}

TEST_CASE("a frame across the loop passes the end's times, then the start's") {
  REQUIRE(crossing({0.05F, 0.95F}, {0.9, 1.1}) == std::vector<size_t>{1, 0});
  // Loops later, the same.
  REQUIRE(crossing({0.05F, 0.95F}, {7.9, 8.1}) == std::vector<size_t>{1, 0});
}

TEST_CASE("a stall passes each time once, not once a loop") {
  REQUIRE(crossing({0.2F, 0.6F}, {0.0, 5.0}) == std::vector<size_t>{0, 1});
}

TEST_CASE("a new clip's first frame passes a time at its very start") {
  REQUIRE(crossing({0.0F}, {-1e-6, 0.016}) == std::vector<size_t>{0});
}

TEST_CASE("nothing is passed standing still, or in a clip of no length") {
  REQUIRE(crossing({0.5F}, {0.5, 0.5}).empty());
  std::vector<size_t> out;
  const std::vector<float> times{0.0F};
  crossedClipTimes(times, 0.0F, {0.0, 1.0}, out);
  REQUIRE(out.empty());
}
