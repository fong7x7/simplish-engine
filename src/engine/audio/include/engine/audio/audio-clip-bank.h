#pragma once

/// @file audio-clip-bank.h
/// @brief Every clip an engine can play, by name.
/// @par Threading
/// Main-thread-only to add to and look up in. A clip's address never
/// changes once added, so the mixer's thread may read a clip it was handed
/// while the main thread adds others.

#include <engine/audio/audio-clip-id.h>
#include <engine/audio/audio-clip.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eng::audio {

/// The clips a game has loaded, each under a name — `shot`, `blast` — and
/// an id that is its place in the order they were added.
///
/// A bank only grows. Taking a clip out while a voice might be playing it
/// would need the mixer's thread to agree first; until levels stream sound
/// in and out, what a game loads it keeps.
class AudioClipBank {
public:
  /// Add @p clip under @p name, or replace the clip already there, and give
  /// its id. Replacing changes what the name plays from the next sound on;
  /// the old clip stays alive for any voice still playing it.
  AudioClipId add(std::string_view name, AudioClip clip);

  /// The id of the clip called @p name, if there is one.
  [[nodiscard]] std::optional<AudioClipId> find(std::string_view name) const;

  /// The clip @p id names, or null when the bank has no such clip.
  [[nodiscard]] const AudioClip* clip(AudioClipId id) const;

  /// How many names the bank holds.
  [[nodiscard]] size_t size() const { return names_.size(); }

private:
  /// Each clip's name, by id.
  std::vector<std::string> names_{};
  /// Each name's current clip, by id; boxed so its address holds still.
  std::vector<std::shared_ptr<const AudioClip>> clips_{};
  /// Clips a name has been moved off, kept for voices still playing them.
  std::vector<std::shared_ptr<const AudioClip>> retired_{};
};

}  // namespace eng::audio
