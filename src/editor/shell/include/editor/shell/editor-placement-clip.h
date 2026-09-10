#pragma once

/// @file editor-placement-clip.h
/// @brief Which clip a placed rigged model plays, and stepping from one
/// clip to the next.
/// @par Threading Thread-safe (pure functions over value types).

#include <cstddef>
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

/// The clip @p steps along from @p current in @p names, wrapping round at
/// either end: what the properties panel's step buttons move to. Empty
/// when there are no names.
[[nodiscard]] std::string stepEditorClip(std::span<const std::string> names,
                                         const std::string& current, int steps);

}  // namespace eng::editor
