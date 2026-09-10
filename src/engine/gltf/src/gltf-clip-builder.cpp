#include "gltf-clip-builder.h"

#include "gltf-accessor.h"
#include "gltf-rig-builder.h"

#include <algorithm>
#include <string>

namespace eng::gltf {

namespace {

  using animation::AnimationChannel;
  using animation::AnimationClip;
  using animation::ChannelInterpolation;
  using animation::ChannelTarget;

  /// What every channel of every animation is read against: the document,
  /// and the node-to-joint map that renumbers it into the skeleton.
  struct ClipSource {
    /// The loaded document.
    const GltfDocument& document;
    /// Skeleton joint per document node, or `GLTF_NOT_A_JOINT`.
    std::span<const uint32_t> joint_of_node;
  };

  /// The pose part glTF path @p path drives, or nothing for morph weights
  /// and anything unknown.
  std::optional<ChannelTarget> channelTarget(const std::string& path) {
    if (path == "translation") {
      return ChannelTarget::TRANSLATION;
    }
    if (path == "rotation") {
      return ChannelTarget::ROTATION;
    }
    if (path == "scale") {
      return ChannelTarget::SCALE;
    }
    return std::nullopt;
  }

  /// glTF's interpolation name as the engine's, `LINEAR` by default.
  ChannelInterpolation interpolation(const std::string& name) {
    if (name == "STEP") {
      return ChannelInterpolation::STEP;
    }
    if (name == "CUBICSPLINE") {
      return ChannelInterpolation::CUBIC_SPLINE;
    }
    return ChannelInterpolation::LINEAR;
  }

  /// The skeleton joint and pose part @p channel drives, if the skeleton
  /// has that joint and the part is one a pose has.
  std::optional<AnimationChannel>
  channelHeader(const Json& channel, std::span<const uint32_t> joint_of_node) {
    const Json* target = jsonMember(channel, "target");
    const auto node = target ? jsonIndex(*target, "node") : std::nullopt;
    const auto part =
        target ? channelTarget(jsonString(*target, "path")) : std::nullopt;
    if (!node || !part || *node >= joint_of_node.size() ||
        joint_of_node[*node] == GLTF_NOT_A_JOINT) {
      return std::nullopt;
    }
    AnimationChannel out;
    out.joint = joint_of_node[*node];
    out.target = *part;
    return out;
  }

  /// Keys per value the interpolation stores: a cubic spline key is an
  /// in-tangent, a value, and an out-tangent.
  size_t valuesPerKey(ChannelInterpolation how) {
    return how == ChannelInterpolation::CUBIC_SPLINE ? 3 : 1;
  }

  /// Read @p sampler's keys into @p out. False when an accessor is bad or
  /// the two disagree about how many keys there are.
  bool readKeys(const GltfDocument& document, const Json& sampler,
                AnimationChannel& out) {
    out.interpolation = interpolation(jsonString(sampler, "interpolation"));
    const auto input = jsonIndex(sampler, "input");
    const auto output = jsonIndex(sampler, "output");
    const size_t n = animation::channelComponents(out.target);
    auto times = input ? readAccessorFloats(document, *input, 1) : std::nullopt;
    auto values =
        output ? readAccessorFloats(document, *output, n) : std::nullopt;
    if (!times || !values || times->empty() ||
        values->size() != times->size() * n * valuesPerKey(out.interpolation)) {
      return false;
    }
    out.times = std::move(*times);
    out.values = std::move(*values);
    return true;
  }

  /// Append @p animation's channel @p index to @p clip. False when it is
  /// malformed; a channel this cannot play is skipped and is not a failure.
  bool appendChannel(const ClipSource& source, const Json& animation,
                     size_t index, AnimationClip& clip) {
    const Json& channel = *jsonElement(animation, "channels", index);
    auto header = channelHeader(channel, source.joint_of_node);
    if (!header) {
      return true;
    }
    const auto sampler = jsonIndex(channel, "sampler");
    const Json* entry =
        sampler ? jsonElement(animation, "samplers", *sampler) : nullptr;
    if (entry == nullptr || !readKeys(source.document, *entry, *header)) {
      return false;
    }
    clip.duration = std::max(clip.duration, header->times.back());
    clip.channels.push_back(std::move(*header));
    return true;
  }

  /// Animation @p index as a clip, or nothing when a channel is malformed.
  std::optional<AnimationClip> buildClip(const ClipSource& source,
                                         size_t index) {
    const Json& animation =
        *jsonElement(source.document.root, "animations", index);
    AnimationClip clip;
    clip.name = jsonString(animation, "name");
    if (clip.name.empty()) {
      clip.name = "animation " + std::to_string(index);
    }
    for (size_t c = 0; c < jsonArraySize(animation, "channels"); ++c) {
      if (!appendChannel(source, animation, c, clip)) {
        return std::nullopt;
      }
    }
    return clip;
  }

}  // namespace

std::expected<std::vector<AnimationClip>, GltfLoadError>
buildGltfClips(const GltfDocument& document,
               std::span<const uint32_t> joint_of_node) {
  const ClipSource source{document, joint_of_node};
  std::vector<AnimationClip> clips;
  for (size_t a = 0; a < jsonArraySize(document.root, "animations"); ++a) {
    auto clip = buildClip(source, a);
    if (!clip) {
      return std::unexpected(GltfLoadError::BAD_ACCESSOR);
    }
    clips.push_back(std::move(*clip));
  }
  return clips;
}

}  // namespace eng::gltf
