#include <engine/audio/audio-clip-bank.h>

namespace eng::audio {

AudioClipId AudioClipBank::add(std::string_view name, AudioClip clip) {
  auto boxed = std::make_shared<const AudioClip>(std::move(clip));
  if (const std::optional<AudioClipId> known = find(name)) {
    retired_.push_back(std::move(clips_[known->index]));
    clips_[known->index] = std::move(boxed);
    return *known;
  }
  names_.emplace_back(name);
  clips_.push_back(std::move(boxed));
  return AudioClipId{static_cast<uint32_t>(clips_.size() - 1)};
}

std::optional<AudioClipId> AudioClipBank::find(std::string_view name) const {
  for (size_t i = 0; i < names_.size(); ++i) {
    if (names_[i] == name) {
      return AudioClipId{static_cast<uint32_t>(i)};
    }
  }
  return std::nullopt;
}

const AudioClip* AudioClipBank::clip(AudioClipId id) const {
  return id.index < clips_.size() ? clips_[id.index].get() : nullptr;
}

}  // namespace eng::audio
