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
