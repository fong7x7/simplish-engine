#include <algorithm>
#include <array>
#include <engine/audio/audio-volumes-json.h>
#include <nlohmann/json.hpp>

namespace eng::audio {

namespace {

  /// Each bus's name, by `AudioBus`.
  constexpr std::array<std::string_view, AUDIO_BUS_COUNT> BUS_NAMES{
      "effects", "music", "interface"};

  /// Read @p json's volume @p name into @p volume, clamped; a missing one
  /// leaves it, and one that is not a number is a problem.
  void readVolume(const nlohmann::json& json, std::string_view name,
                  float& volume, AudioVolumesLoad& out) {
    const auto found = json.find(name);
    if (found == json.end()) {
      return;
    }
    if (!found->is_number()) {
      out.problems.push_back(std::string{name} + " is not a number");
      return;
    }
    volume = std::clamp(found->get<float>(), 0.0F, 1.0F);
  }

  /// Read @p json's `muted` into @p out.
  void readMuted(const nlohmann::json& json, AudioVolumesLoad& out) {
    const auto found = json.find("muted");
    if (found == json.end()) {
      return;
    }
    if (!found->is_boolean()) {
      out.problems.emplace_back("muted is not true or false");
      return;
    }
    out.volumes.muting =
        found->get<bool>() ? AudioMuting::MUTED : AudioMuting::AUDIBLE;
  }

  /// Report every key in @p json that names nothing.
  void readUnknown(const nlohmann::json& json, AudioVolumesLoad& out) {
    for (const auto& [key, value] : json.items()) {
      if (key != "master" && key != "muted" && !audioBusNamed(key)) {
        out.problems.push_back("no volume called " + key);
      }
    }
  }

}  // namespace

std::string_view audioBusName(AudioBus bus) {
  return BUS_NAMES[static_cast<size_t>(bus)];
}

std::optional<AudioBus> audioBusNamed(std::string_view name) {
  const auto* found = std::ranges::find(BUS_NAMES, name);
  if (found == BUS_NAMES.end()) {
    return std::nullopt;
  }
  return static_cast<AudioBus>(found - BUS_NAMES.begin());
}

std::string writeAudioVolumes(const AudioVolumes& volumes) {
  nlohmann::ordered_json json;
  json["master"] = volumes.master;
  for (uint8_t i = 0; i < AUDIO_BUS_COUNT; ++i) {
    const auto bus = static_cast<AudioBus>(i);
    json[std::string{audioBusName(bus)}] = busVolume(volumes, bus);
  }
  json["muted"] = volumes.muting == AudioMuting::MUTED;
  return json.dump(2) + "\n";
}

AudioVolumesLoad parseAudioVolumes(std::string_view text) {
  AudioVolumesLoad out;
  const nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
  if (!json.is_object()) {
    out.problems.emplace_back("not a JSON object of volumes");
    return out;
  }
  readVolume(json, "master", out.volumes.master, out);
  for (uint8_t i = 0; i < AUDIO_BUS_COUNT; ++i) {
    readVolume(json, BUS_NAMES[i], out.volumes.buses[i], out);
  }
  readMuted(json, out);
  readUnknown(json, out);
  return out;
}

}  // namespace eng::audio
