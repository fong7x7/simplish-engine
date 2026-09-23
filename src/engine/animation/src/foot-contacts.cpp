#include <algorithm>
#include <cctype>
#include <cmath>
#include <engine/animation/foot-contacts.h>
#include <engine/animation/pose-sampling.h>
#include <string>
#include <string_view>

namespace eng::animation {

namespace {

  /// @p name in lower case.
  std::string lowered(std::string_view name) {
    std::string out(name);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return out;
  }

  /// Whether a joint called @p name is a foot and not a helper for one.
  bool isFoot(std::string_view name) {
    const std::string low = lowered(name);
    const bool foot = low.contains("foot") || low.contains("ankle");
    constexpr std::string_view HELPERS[] = {"end", "ik", "target", "pole"};
    return foot && std::ranges::none_of(HELPERS, [&low](std::string_view h) {
             return low.contains(h);
           });
  }

  /// How high a joint whose skeleton-space transform is @p world stands in
  /// the model: after @p rig's skin root, which is where a Y-up file is
  /// turned upright (`orientSkinnedYUpToZUp`), so up is Z whatever the
  /// source's convention.
  float heightOf(const Rig& rig, const Mat4& world) {
    return (rig.skin.root * world)(2, 3);
  }

  /// World heights of @p joints — one row each — at @p samples even steps
  /// across clip @p clip of @p rig.
  std::vector<std::vector<float>>
  sampleHeights(const Rig& rig, size_t clip, std::span<const uint32_t> joints,
                size_t samples) {
    const size_t count = rig.skeleton.parents.size();
    std::vector<JointPose> pose(count);
    std::vector<Mat4> worlds(count);
    std::vector<std::vector<float>> heights(joints.size(),
                                            std::vector<float>(samples));
    const float step = rig.clips[clip].duration / static_cast<float>(samples);
    for (size_t i = 0; i < samples; ++i) {
      samplePose(rig.skeleton, rig.clips[clip], step * static_cast<float>(i),
                 pose);
      computeJointWorlds(rig.skeleton, pose, worlds);
      for (size_t j = 0; j < joints.size(); ++j) {
        heights[j][i] = heightOf(rig, worlds[joints[j]]);
      }
    }
    return heights;
  }

  /// How tall @p rig's skeleton stands at rest: its joints' spread in
  /// height.
  float restHeight(const Rig& rig) {
    std::vector<Mat4> worlds(rig.skeleton.parents.size());
    computeJointWorlds(rig.skeleton, rig.skeleton.rest, worlds);
    std::vector<float> heights;
    heights.reserve(worlds.size());
    for (const Mat4& world : worlds) {
      heights.push_back(heightOf(rig, world));
    }
    const auto [low, high] = std::ranges::minmax(heights);
    return high - low;
  }

  /// Where the foot whose heights are @p row comes down, each a fraction of
  /// the way through the clip, into @p out — as seconds, @p step apart.
  void contactsOf(std::span<const float> row, float step, float min_travel,
                  std::vector<float>& out) {
    const auto [lo, hi] = std::ranges::minmax(row);
    if (hi - lo < min_travel) {
      return;
    }
    const float band = lo + (hi - lo) * FOOT_CONTACT_BAND;
    for (size_t i = 0; i < row.size(); ++i) {
      const float before = row[(i + row.size() - 1) % row.size()];
      if (before > band && row[i] <= band) {
        const float into = (before - band) / (before - row[i]);
        const float at = (static_cast<float>(i) - 1.0F + into) * step;
        out.push_back(at < 0.0F ? at + step * static_cast<float>(row.size())
                                : at);
      }
    }
  }

}  // namespace

std::vector<uint32_t> findFootJoints(const Skeleton& skeleton) {
  std::vector<uint32_t> feet;
  for (uint32_t j = 0; j < skeleton.names.size(); ++j) {
    if (isFoot(skeleton.names[j])) {
      feet.push_back(j);
    }
  }
  return feet;
}

std::vector<float> detectFootContacts(const Rig& rig, size_t clip) {
  const std::vector<uint32_t> feet = findFootJoints(rig.skeleton);
  if (feet.empty() || clip >= rig.clips.size() ||
      rig.clips[clip].duration <= 0.0F || !skeletonIsOrdered(rig.skeleton)) {
    return {};
  }
  const float duration = rig.clips[clip].duration;
  const auto samples = static_cast<size_t>(
      std::max(8.0F, std::ceil(duration * FOOT_CONTACT_SAMPLES_PER_SECOND)));
  const float min_travel = restHeight(rig) * FOOT_CONTACT_MIN_TRAVEL;
  std::vector<float> contacts;
  for (const auto& row : sampleHeights(rig, clip, feet, samples)) {
    contactsOf(row, duration / static_cast<float>(samples), min_travel,
               contacts);
  }
  std::ranges::sort(contacts);
  return contacts;
}

}  // namespace eng::animation
