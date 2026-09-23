#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <engine/animation/foot-contacts.h>
#include <utility>

using Catch::Approx;
using namespace eng;
using namespace eng::animation;

namespace {

/// A hip and a foot under it, the foot's height keyed at @p heights over
/// one second: a walk that lifts it, or an idle that does not.
Rig oneFootRig(std::vector<float> heights) {
  Rig rig;
  rig.skeleton.parents = {SKELETON_NO_PARENT, 0};
  rig.skeleton.rest = {JointPose{.translation = {0, 0, 1}},
                       JointPose{.translation = {0, 0, -1}}};
  rig.skeleton.names = {"Hips", "LeftFoot"};
  AnimationChannel lift;
  lift.joint = 1;
  lift.target = ChannelTarget::TRANSLATION;
  for (size_t i = 0; i < heights.size(); ++i) {
    lift.times.push_back(static_cast<float>(i) /
                         static_cast<float>(heights.size() - 1));
    lift.values.insert(lift.values.end(), {0.0F, 0.0F, heights[i] - 1.0F});
  }
  rig.clips.push_back({"walk", 1.0F, {lift}});
  return rig;
}

}  // namespace

TEST_CASE("feet are found by name, and their helpers are not") {
  Skeleton skeleton;
  skeleton.names = {"Hips",      "LeftFoot",   "foot.R", "LeftFoot_end",
                    "foot_ik.L", "RightAnkle", "LeftToe"};
  REQUIRE(findFootJoints(skeleton) == std::vector<uint32_t>{1, 2, 5});
}

TEST_CASE("a foot comes down as it drops into the bottom of its travel") {
  // Up, halfway, down, halfway, up: lowest at half a second.
  const Rig rig = oneFootRig({1.0F, 0.5F, 0.0F, 0.5F, 1.0F});
  const std::vector<float> contacts = detectFootContacts(rig, 0);
  REQUIRE(contacts.size() == 1);
  // Down through a fifth of its travel between the quarter and the half.
  REQUIRE(contacts[0] == Approx(0.4F).margin(0.01F));
}

TEST_CASE("a foot that barely moves takes no steps") {
  const Rig rig = oneFootRig({0.0F, 0.001F, 0.0F, 0.001F, 0.0F});
  REQUIRE(detectFootContacts(rig, 0).empty());
}

TEST_CASE("a rig without feet, or a clip it lacks, has no contacts") {
  Rig rig = oneFootRig({1.0F, 0.0F, 1.0F});
  REQUIRE(detectFootContacts(rig, 4).empty());
  rig.skeleton.names[1] = "LeftHand";
  REQUIRE(detectFootContacts(rig, 0).empty());
}

namespace {

/// `oneFootRig` keyed along Y rather than Z, as a glTF file keys it.
Rig yUpRig() {
  Rig rig = oneFootRig({1.0F, 0.5F, 0.0F, 0.5F, 1.0F});
  for (JointPose& pose : rig.skeleton.rest) {
    std::swap(pose.translation.y, pose.translation.z);
  }
  std::vector<float>& values = rig.clips[0].channels[0].values;
  for (size_t i = 0; i + 2 < values.size(); i += 3) {
    std::swap(values[i + 1], values[i + 2]);
  }
  return rig;
}

}  // namespace

TEST_CASE("a Y-up rig turned upright by its skin root steps the same") {
  // Up is Y in the skeleton; only the skin's root, turning Y into Z as the
  // loader's orientation does, makes it up in the model.
  Rig rig = yUpRig();
  REQUIRE(detectFootContacts(rig, 0).empty());

  rig.skin.root = Mat4{};
  rig.skin.root(0, 0) = 1.0F;
  rig.skin.root(2, 1) = 1.0F;
  rig.skin.root(1, 2) = -1.0F;
  rig.skin.root(3, 3) = 1.0F;
  const std::vector<float> contacts = detectFootContacts(rig, 0);
  REQUIRE(contacts.size() == 1);
  REQUIRE(contacts[0] == Approx(0.4F).margin(0.01F));
}
