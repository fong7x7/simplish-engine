#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/core/pcg32.h>
#include <engine/physics/box-broadphase.h>
#include <engine/physics/cylinder-collision.h>
#include <vector>

using eng::Pcg32;
using eng::Vec2;
using eng::physics::BoxBroadphase;
using eng::physics::CollisionBox;
using eng::physics::CollisionCylinder;
using eng::physics::resolveCylinderAgainstBoxes;

namespace {

/// Crates of every size scattered over 40 × 40 tiles, and a long wall.
std::vector<CollisionBox> scatteredBoxes() {
  Pcg32 rng(11, 11);
  std::vector<CollisionBox> boxes{{{-20, -20, 0}, {20, -19, 2}}};
  for (int i = 0; i < 200; ++i) {
    const float x = rng.nextUnitFloat() * 40.0F - 20.0F;
    const float y = rng.nextUnitFloat() * 40.0F - 20.0F;
    const float side = 0.25F + rng.nextUnitFloat() * 3.0F;
    boxes.push_back({{x, y, 0}, {x + side, y + side, 1}});
  }
  return boxes;
}

/// What `gather` lists for a point, reserved as a caller would.
std::vector<uint32_t> gathered(const BoxBroadphase& broadphase, Vec2 at,
                               float reach) {
  std::vector<uint32_t> out;
  out.reserve(broadphase.candidateCapacity());
  broadphase.gather(at, reach, out);
  return out;
}

/// Whether @p box's footprint touches the square from @p low to @p high.
bool touchesSquare(const CollisionBox& box, Vec2 low, Vec2 high) {
  return box.min.x <= high.x && box.max.x >= low.x && box.min.y <= high.y &&
         box.max.y >= low.y;
}

/// A player-sized cylinder somewhere over the scattered boxes, or just
/// past them.
CollisionCylinder playerSomewhere(Pcg32& rng) {
  const float x = rng.nextUnitFloat() * 44.0F - 22.0F;
  const float y = rng.nextUnitFloat() * 44.0F - 22.0F;
  return {{x, y}, 0.3F, 0.0F, 1.5F};
}

}  // namespace

TEST_CASE("a gather lists every box near the point, ascending, once each") {
  const std::vector<CollisionBox> boxes = scatteredBoxes();
  const BoxBroadphase broadphase(boxes);
  const std::vector<uint32_t> near = gathered(broadphase, {3, 4}, 1.0F);

  REQUIRE(std::ranges::is_sorted(near));
  REQUIRE(std::ranges::adjacent_find(near) == near.end());
  REQUIRE(near.size() < boxes.size() / 4);
  for (uint32_t i = 0; i < boxes.size(); ++i) {
    if (touchesSquare(boxes[i], {2, 3}, {4, 5})) {
      REQUIRE(std::ranges::binary_search(near, i));
    }
  }
}

TEST_CASE("resolving against the candidates is resolving against them all") {
  const std::vector<CollisionBox> boxes = scatteredBoxes();
  const BoxBroadphase broadphase(boxes);
  Pcg32 rng(5, 5);
  for (int i = 0; i < 2000; ++i) {
    const CollisionCylinder cylinder = playerSomewhere(rng);
    const Vec2 full = resolveCylinderAgainstBoxes(cylinder, boxes);
    const Vec2 fast = resolveCylinderAgainstBoxes(
        cylinder, boxes, gathered(broadphase, cylinder.center, 1.3F));
    // A cylinder spawned deep inside a big crate is pushed further than
    // the gather reaches; everything else must match to the bit.
    if (Vec2::distanceSquared(full, cylinder.center) < 1.0F) {
      REQUIRE(fast.x == full.x);
      REQUIRE(fast.y == full.y);
    }
  }
}

TEST_CASE("a broadphase of no boxes gathers nothing") {
  const BoxBroadphase broadphase;
  REQUIRE(gathered(broadphase, {0, 0}, 5.0F).empty());
}

TEST_CASE("a point off the level gathers the boxes at the edge it is past") {
  const std::vector<CollisionBox> boxes{{{0, 0, 0}, {1, 1, 1}},
                                        {{10, 0, 0}, {11, 1, 1}}};
  const BoxBroadphase broadphase(boxes);
  REQUIRE(gathered(broadphase, {40, 0.5F}, 1.0F) == std::vector<uint32_t>{1});
}
