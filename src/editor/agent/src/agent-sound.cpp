#include "agent-sound.h"

#include "agent-call.h"

#include <algorithm>
#include <array>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-sound-import.h>
#include <editor/shell/editor-sound-ops.h>
#include <engine/audio/audio-volumes-json.h>
#include <optional>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// The volumes `set_volume` takes, and `get_sound` reports: the master,
  /// then each bus by its name.
  constexpr std::array<std::string_view, 1 + audio::AUDIO_BUS_COUNT>
      VOLUME_KEYS{"master", "effects", "music", "interface"};

  /// The volume @p key names in @p volumes.
  float& volumeAt(audio::AudioVolumes& volumes, std::string_view key) {
    const std::optional<audio::AudioBus> bus = audio::audioBusNamed(key);
    return bus ? volumes.buses[static_cast<size_t>(*bus)] : volumes.master;
  }

  /// Every slot, with its name and the file it plays.
  json slotsJson(const EditorShellState& state) {
    json slots = json::array();
    for (const std::string& slot : editorSoundSlots()) {
      const std::filesystem::path file =
          editorAssignedSound(state.sounds, slot);
      const bool listed =
          std::ranges::find(state.sound_files, file) != state.sound_files.end();
      slots.push_back(
          {{"slot", slot},
           {"name", editorSoundSlotLabel(slot)},
           {"file", file.empty() ? json(nullptr) : json(file.generic_string())},
           {"missing", !file.empty() && !listed}});
    }
    return slots;
  }

  /// The project's sound files, as `set_sound` takes them.
  json filesJson(const EditorShellState& state) {
    json files = json::array();
    for (const std::filesystem::path& file : state.sound_files) {
      files.push_back(file.generic_string());
    }
    return files;
  }

  /// The volumes, as `get_sound` reports them.
  json volumesJson(const EditorShellState& state) {
    json volumes = json::parse(audio::writeAudioVolumes(state.sound.volumes));
    volumes["file"] = state.sound.file.string();
    return volumes;
  }

  /// Everything `get_sound` reports.
  std::string soundPayload(const EditorShellState& state) {
    json root{{"volumes", volumesJson(state)},
              {"slots", slotsJson(state)},
              {"sound_files", filesJson(state)},
              {"problems", state.sounds.problems}};
    root["table"] =
        state.project.loaded
            ? json(editorSoundTablePath(state.project.root).string())
            : json(nullptr);
    return root.dump();
  }

  /// Read the call's volume @p key into @p volumes; a failure when it is
  /// there and is not a number from 0 to 1.
  std::optional<AgentResult> readVolume(audio::AudioVolumes& volumes,
                                        const json& params,
                                        std::string_view key) {
    if (!params.contains(key)) {
      return std::nullopt;
    }
    const std::optional<double> value = agentNumberParam(params, key);
    if (!value || *value < 0.0 || *value > 1.0) {
      return agentFailure(AgentStatus::BAD_PARAMS,
                          std::string{key} + " must be a number from 0 to 1");
    }
    volumeAt(volumes, key) = static_cast<float>(*value);
    return std::nullopt;
  }

  /// The slot the call's `slot` names, or a failure.
  std::optional<std::string> slotParam(const json& params) {
    const std::optional<std::string> name = agentStringParam(params, "slot");
    return name ? findEditorSoundSlot(*name) : std::nullopt;
  }

  /// A failure for a call whose `slot` names nothing.
  AgentResult noSuchSlot() {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "slot names no sound; get_sound lists them");
  }

  /// Whether @p file is one of the project's sound files, or empty.
  bool knownFile(const EditorShellState& state, const std::string& file) {
    return file.empty() ||
           std::ranges::find(state.sound_files, std::filesystem::path(file)) !=
               state.sound_files.end();
  }

  /// Why `import_sound` cannot run as called, or nothing when it can.
  std::optional<AgentResult> importRefusal(const EditorShellState& state,
                                           const json& params) {
    if (!state.project.loaded) {
      return agentFailure(AgentStatus::UNAVAILABLE, "no project is open");
    }
    if (!agentStringParam(params, "path")) {
      return agentFailure(AgentStatus::BAD_PARAMS, "path is required");
    }
    if (params.contains("slot") && !slotParam(params)) {
      return noSuchSlot();
    }
    return std::nullopt;
  }

}  // namespace

AgentResult runAgentGetSound(EditorShellState& state, const json& /*params*/) {
  return agentOk(soundPayload(state));
}

AgentResult runAgentSetVolume(EditorShellState& state, const json& params) {
  audio::AudioVolumes next = state.sound.volumes;
  for (const std::string_view key : VOLUME_KEYS) {
    if (std::optional<AgentResult> refused = readVolume(next, params, key)) {
      return *refused;
    }
  }
  if (const std::optional<bool> muted = agentBoolParam(params, "muted")) {
    next.muting =
        *muted ? audio::AudioMuting::MUTED : audio::AudioMuting::AUDIBLE;
  }
  state.sound.volumes = next;
  ++state.sound.revision;
  return agentOk(soundPayload(state));
}

AgentResult runAgentSetSound(EditorShellState& state, const json& params) {
  if (!state.project.loaded) {
    return agentFailure(AgentStatus::UNAVAILABLE, "no project is open");
  }
  const std::optional<std::string> slot = slotParam(params);
  const std::optional<std::string> file = agentStringParam(params, "file");
  if (!slot) {
    return noSuchSlot();
  }
  if (!file || !knownFile(state, *file)) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "file must be one of get_sound's sound_files, or "
                        "empty for the built-in sound");
  }
  assignEditorSound(state.sounds, *slot, std::filesystem::path(*file));
  ++state.sounds.revision;
  return agentOk(soundPayload(state));
}

AgentResult runAgentImportSound(EditorShellState& state, const json& params) {
  if (std::optional<AgentResult> refused = importRefusal(state, params)) {
    return *refused;
  }
  const std::string path = agentStringParam(params, "path").value_or("");
  const std::optional<std::string> slot = slotParam(params);
  const std::filesystem::path assets = projectAssetsPath(state.project.root);
  const EditorSoundImport imported = importEditorSound(assets, path);
  if (!imported.error.empty()) {
    return agentFailure(AgentStatus::BAD_PARAMS, imported.error);
  }
  state.sound_files = scanEditorAssets(assets).sounds;
  if (slot) {
    assignEditorSound(state.sounds, *slot, imported.file);
    ++state.sounds.revision;
  }
  return agentOk(soundPayload(state));
}

AgentResult runAgentPlaySound(EditorShellState& state, const json& params) {
  const std::optional<std::string> name = agentStringParam(params, "name");
  if (!name || (!findEditorSoundSlot(*name) &&
                (name->empty() || !knownFile(state, *name)))) {
    return agentFailure(AgentStatus::NOT_FOUND,
                        "name must be a slot or one of get_sound's "
                        "sound_files");
  }
  AgentResult result = agentOk(json{{"playing", *name}}.dump());
  result.host.kind = AgentHostRequestKind::PLAY_SOUND;
  result.host.sound = *name;
  return result;
}

}  // namespace eng::editor
