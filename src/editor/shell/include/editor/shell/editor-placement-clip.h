#pragma once

/// @file editor-placement-clip.h
/// @brief Which clip a placed rigged model plays, and which one a model
/// drawn as a player plays.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
#include <editor/shell/editor-character-gait.h>
#include <editor/shell/editor-placement.h>
#include <engine/animation/rig.h>
#include <span>
#include <string>
#include <vector>

namespace eng::editor {

/// The names of @p rig's clips, in the order its file lists them. Empty
/// for no rig, or a rig with no clips.
[[nodiscard]] std::vector<std::string>
editorClipNames(const animation::Rig* rig);

/// Where clip @p name is in @p names, or 0 — the first clip — when it is
/// empty or names none of them. What a placement's `animation` means.
[[nodiscard]] size_t editorClipIndex(std::span<const std::string> names,
                                     const std::string& name);

/// The clip index @p placement plays in @p rig, as `RigPose::evaluate`
/// takes it: the clip it names, or the first. `RIG_REST_POSE` when there
/// is no rig or it has no clips, which draws the model as it was modelled.
[[nodiscard]] size_t editorPlacementClip(const EditorPlacement& placement,
                                         const animation::Rig* rig);

/// The clip a model drawn as a player plays when @p gait: the first whose
/// name has `run` or `walk` in it while moving, the first with `idle` in
/// it while still, matched case-insensitively. Empty — the model's first
/// clip — when none is named so.
///
/// By name because that is all a file says about a clip, and those three
/// are what almost every character pack calls them. A character that
/// needs more than walking and standing needs the animation state machine
/// of Engine REQUIREMENTS §6, not a longer list here.
[[nodiscard]] std::string
editorCharacterClip(std::span<const std::string> names,
                    EditorCharacterGait gait);

}  // namespace eng::editor
