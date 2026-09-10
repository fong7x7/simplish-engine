#pragma once

/// @file pose-sampling.h
/// @brief Turning a clip and a time into joint poses, and joint poses into
/// the matrices a skinned mesh is drawn with.
/// @par Threading
/// Pure functions over caller-owned storage.
///
/// Three steps, each its own function so each can be tested alone and so
/// a caller that blends two clips later has somewhere to stand between
/// them:
///
///   1. `samplePose`         clip + time  → local pose per joint
///   2. `computeJointWorlds` local poses  → world matrix per joint
///   3. `computeSkinMatrices` world matrices → one matrix per skin joint,
///                                             taking a bind-pose vertex to
///                                             where that joint has moved it
///
/// None of them allocates. `RigPose` owns the storage and runs all three.
///
/// `blendPoses` sits between steps 1 and 2 when two clips are playing at
/// once: sample each into its own pose, blend, and carry on from the blend.
/// `ClipPlayer` does that for a crossfade.

#include <engine/animation/animation-channel.h>
#include <engine/animation/animation-clip.h>
#include <engine/animation/joint-pose.h>
#include <engine/animation/skeleton.h>
#include <engine/animation/skin.h>
#include <engine/math/mat4.h>
#include <span>

namespace eng::animation {

/// Where @p elapsed seconds of looping playback lands inside @p clip: the
/// remainder after whole loops, so a clip plays from zero to its duration
/// and starts again. A clip with no length is always at zero.
[[nodiscard]] float loopClipTime(const AnimationClip& clip, double elapsed);

/// The value of @p channel at @p seconds, written to the first
/// `channelComponents(channel.target)` floats of @p out.
///
/// Before the first key a channel holds that key's value, and after the
/// last it holds the last one's. A channel with no keys writes nothing.
void sampleChannel(const AnimationChannel& channel, float seconds,
                   std::span<float, 4> out);

/// Every joint's local pose @p seconds into @p clip: the skeleton's rest
/// pose, with each of the clip's channels written over the part it drives.
///
/// @p pose must be as long as the skeleton. A channel naming a joint past
/// its end is skipped rather than written out of bounds.
void samplePose(const Skeleton& skeleton, const AnimationClip& clip,
                float seconds, std::span<JointPose> pose);

/// Two sets of local poses mixed @p weight of the way from @p from to @p to:
/// translations and scales in a straight line, rotations at constant speed
/// along the shorter arc. Zero gives @p from and one gives @p to; the weight
/// is clamped to that range.
///
/// Blending is done on local poses, joint by joint, never on the world or
/// skin matrices: two arms swung either side of a body average to an arm
/// hanging down, where averaging their matrices would shrink the arm.
/// @p out may be either input. Only as many joints as all three spans hold
/// are written.
void blendPoses(std::span<const JointPose> from, std::span<const JointPose> to,
                float weight, std::span<JointPose> out);

/// Each joint's world transform — its local pose composed with every
/// ancestor's — in one forward pass, which is correct because a skeleton
/// orders every parent before its children.
///
/// @p pose and @p worlds must both be as long as the skeleton.
void computeJointWorlds(const Skeleton& skeleton,
                        std::span<const JointPose> pose,
                        std::span<Mat4> worlds);

/// The matrix each skin joint moves its vertices by: the skin's root, then
/// the joint's world transform, then its inverse bind matrix. A vertex in
/// the bind pose, multiplied by this, lands where the posed joint carries
/// it — and in the bind pose itself, every one of these is the identity.
///
/// @p out must be as long as the skin. A skin joint naming a skeleton joint
/// past the end of @p worlds is left at the identity, so a malformed skin
/// draws its mesh unmoved rather than reading out of bounds.
void computeSkinMatrices(const Skin& skin, std::span<const Mat4> worlds,
                         std::span<Mat4> out);

}  // namespace eng::animation
