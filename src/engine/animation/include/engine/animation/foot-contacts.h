#pragma once

/// @file foot-contacts.h
/// @brief When a clip puts each foot down, found from the skeleton.
/// @par Threading
/// Pure functions.

#include <cstddef>
#include <cstdint>
#include <engine/animation/rig.h>
#include <vector>

namespace eng::animation {

/// How finely a clip is sampled to find its contacts, in samples a second.
inline constexpr float FOOT_CONTACT_SAMPLES_PER_SECOND = 120.0F;

/// How far up a foot's travel, as a fraction of it, counts as down: a foot
/// is planted from when it drops below this until it next rises above.
inline constexpr float FOOT_CONTACT_BAND = 0.2F;

/// How far a foot has to travel up and down in a clip, as a fraction of the
/// skeleton's height at rest, before its lowest points count as steps. An
/// idle's feet shift a little; a walk's lift.
inline constexpr float FOOT_CONTACT_MIN_TRAVEL = 0.03F;

/// The joints of @p skeleton that are feet: named with `foot` or `ankle`,
/// whatever the case, and not an exporter's helper for one — a name with
/// `end`, `ik`, `target` or `pole` in it. In skeleton order.
[[nodiscard]] std::vector<uint32_t> findFootJoints(const Skeleton& skeleton);

/// Seconds into clip @p clip of @p rig at which a foot comes down, sorted:
/// for each foot joint whose height (world Z, up) travels far enough, every
/// moment it drops into the lowest `FOOT_CONTACT_BAND` of its travel,
/// looping round the clip's end. Empty for a rig with no feet, a clip past
/// its clips, or one that does not lift a foot — an idle.
[[nodiscard]] std::vector<float> detectFootContacts(const Rig& rig,
                                                    size_t clip);

}  // namespace eng::animation
