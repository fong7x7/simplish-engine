#include <catch2/catch_test_macros.hpp>
#include <engine/animation/skeleton.h>

using eng::animation::Skeleton;
using eng::animation::SKELETON_NO_PARENT;
using eng::animation::skeletonIsOrdered;

namespace {

/// A root, a child of it, and a grandchild: the shape of an arm.
Skeleton arm() {
  Skeleton s;
  s.parents = {SKELETON_NO_PARENT, 0, 1};
  s.rest.resize(3);
  s.names = {"shoulder", "elbow", "wrist"};
  return s;
}

}  // namespace

TEST_CASE("a skeleton whose parents precede their children is ordered") {
  REQUIRE(skeletonIsOrdered(arm()));
  REQUIRE(skeletonIsOrdered(Skeleton{}));
}

TEST_CASE("a skeleton with two roots is still ordered") {
  Skeleton s = arm();
  s.parents[1] = SKELETON_NO_PARENT;
  REQUIRE(skeletonIsOrdered(s));
}

TEST_CASE("a parent after its child breaks the order") {
  Skeleton s = arm();
  s.parents[1] = 2;
  REQUIRE_FALSE(skeletonIsOrdered(s));
}

TEST_CASE("a joint that is its own parent breaks the order") {
  Skeleton s = arm();
  s.parents[2] = 2;
  REQUIRE_FALSE(skeletonIsOrdered(s));
}

TEST_CASE("arrays of different lengths are not a skeleton") {
  Skeleton s = arm();
  s.names.pop_back();
  REQUIRE_FALSE(skeletonIsOrdered(s));
}
