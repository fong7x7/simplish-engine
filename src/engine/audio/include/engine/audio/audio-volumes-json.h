#pragma once

/// @file audio-volumes-json.h
/// @brief Volume settings to and from the JSON a player can edit.
/// @par Threading
/// Pure functions.

#include <engine/audio/audio-bus.h>
#include <engine/audio/audio-volumes-load.h>
#include <engine/audio/audio-volumes.h>
#include <optional>
#include <string>
#include <string_view>

namespace eng::audio {

/// @p bus's name in a settings file and to an agent: "effects", "music",
/// "interface".
[[nodiscard]] std::string_view audioBusName(AudioBus bus);

/// The bus named @p name, or nothing.
[[nodiscard]] std::optional<AudioBus> audioBusNamed(std::string_view name);

/// @p volumes as JSON: `master`, one number per bus by its name, each 0 to
/// 1, and `muted`.
[[nodiscard]] std::string writeAudioVolumes(const AudioVolumes& volumes);

/// The settings @p text describes, over the defaults: a volume the file
/// leaves out stays at full, a number outside 0 to 1 is clamped, and
/// anything unreadable is skipped and reported. Text that is not JSON at all
/// gives the defaults and one problem.
[[nodiscard]] AudioVolumesLoad parseAudioVolumes(std::string_view text);

}  // namespace eng::audio
