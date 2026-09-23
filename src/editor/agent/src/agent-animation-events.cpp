#include "agent-animation-events.h"

#include "agent-call.h"

#include <algorithm>
#include <editor/shell/editor-animation-event-ops.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-sound-ops.h>
#include <optional>
#include <string>
#include <vector>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// What `set_animation_events` says when it is called wrongly.
  constexpr std::string_view SET_USAGE =
      "name a rigged model's clip (asset and clip) or a sprite sheet "
      "(sheet), and give events as a list of {at or frame, sound, gain}; "
      "sound is footstep, a slot get_sound lists, or one of its "
      "sound_files; a clip's events fall within its duration. Omit events "
      "to take the row away.";

  /// The loudest an event may be.
  constexpr double MAX_GAIN = 4.0;

  /// What @p source is called to an agent.
  std::string_view sourceName(EditorEventSource source) {
    switch (source) {
      case EditorEventSource::NONE:
        return "none";
      case EditorEventSource::DETECTED:
        return "detected";
      case EditorEventSource::AUTHORED:
        return "authored";
    }
    return "none";
  }

  /// @p events as this API reports them.
  json eventsJson(const std::vector<EditorAnimationEvent>& events) {
    json out = json::array();
    for (const EditorAnimationEvent& event : events) {
      out.push_back(
          {{"at", event.at}, {"sound", event.sound}, {"gain", event.gain}});
    }
    return out;
  }

  /// Append every clip of @p asset's rig, and what it plays — worked out
  /// from @p state's table as it stands, so a write is seen at once.
  void appendAssetClips(json& clips, const EditorShellState& state,
                        const EditorAsset& asset) {
    const std::string ref = editorAssetRef(asset);
    const auto sets =
        resolveEditorClipEvents(state.animation_events, ref, *asset.rig);
    for (size_t i = 0; i < sets.size(); ++i) {
      clips.push_back({{"asset", ref},
                       {"clip", asset.rig->clips[i].name},
                       {"duration", asset.rig->clips[i].duration},
                       {"source", sourceName(sets[i].source)},
                       {"events", eventsJson(sets[i].events)}});
    }
  }

  /// Every clip of every loaded rig, and what it plays.
  json clipsJson(const EditorShellState& state) {
    json clips = json::array();
    for (const EditorAsset& asset : state.assets) {
      if (asset.rig != nullptr) {
        appendAssetClips(clips, state, asset);
      }
    }
    return clips;
  }

  /// The table's sheet rows, as this API reports them.
  json sheetsJson(const EditorShellState& state) {
    json sheets = json::array();
    for (const EditorSheetEventEntry& entry : state.animation_events.sheets) {
      json events = json::array();
      for (const EditorFrameEvent& event : entry.events) {
        events.push_back({{"frame", event.frame},
                          {"sound", event.sound},
                          {"gain", event.gain}});
      }
      sheets.push_back({{"sheet", entry.sheet}, {"events", events}});
    }
    return sheets;
  }

  /// The table's clip rows, including models not loaded yet.
  json authoredJson(const EditorShellState& state) {
    json rows = json::array();
    for (const EditorClipEventEntry& entry : state.animation_events.clips) {
      rows.push_back({{"asset", entry.asset},
                      {"clip", entry.clip},
                      {"events", eventsJson(entry.events)}});
    }
    return rows;
  }

  /// Everything `list_animation_events` reports.
  std::string payload(const EditorShellState& state) {
    return json{{"clips", clipsJson(state)},
                {"authored_clips", authoredJson(state)},
                {"sheets", sheetsJson(state)},
                {"problems", state.animation_events.problems}}
        .dump(2);
  }

  /// Whether @p sound is one an event may play in @p state's project.
  bool playable(const EditorShellState& state, const std::string& sound) {
    return sound == EDITOR_FOOTSTEP_EVENT || findEditorSoundSlot(sound) ||
           std::ranges::find(state.sound_files, std::filesystem::path(sound)) !=
               state.sound_files.end();
  }

  /// The sound and gain of event @p item, or nothing when either is wrong.
  std::optional<std::pair<std::string, float>>
  soundOf(const EditorShellState& state, const json& item) {
    const std::string sound = agentStringParam(item, "sound").value_or("");
    const double gain = agentNumberParam(item, "gain").value_or(1.0);
    if (!playable(state, sound) || gain < 0.0 || gain > MAX_GAIN) {
      return std::nullopt;
    }
    return std::pair{sound, static_cast<float>(gain)};
  }

  /// Each of the call's events read by @p read, or nothing when any is
  /// wrong.
  template <typename Event, typename Read>
  std::optional<std::vector<Event>> readEvents(const json& params, Read read) {
    const auto found = params.find("events");
    if (found == params.end() || !found->is_array()) {
      return std::nullopt;
    }
    std::vector<Event> events;
    for (const json& item : *found) {
      std::optional<Event> event =
          item.is_object() ? read(item) : std::optional<Event>{};
      if (!event) {
        return std::nullopt;
      }
      events.push_back(std::move(*event));
    }
    return events;
  }

  /// The asset the call's `asset` names — by reference, id or name — or
  /// nothing when it names no rigged model.
  const EditorAsset* riggedAsset(const EditorShellState& state,
                                 const std::string& name) {
    const auto found = std::ranges::find_if(state.assets, [&](const auto& a) {
      return editorAssetRef(a) == name || a.id == name || a.name == name;
    });
    return found != state.assets.end() && !found->shape &&
                   isRiggedModelFile(found->path)
               ? &*found
               : nullptr;
  }

  /// Whether @p asset has a clip @p clip, or cannot say because its rig is
  /// not loaded yet.
  bool mayHaveClip(const EditorAsset& asset, const std::string& clip) {
    return asset.rig == nullptr ||
           std::ranges::find(asset.rig->clips, clip,
                             &animation::AnimationClip::name) !=
               asset.rig->clips.end();
  }

  /// One clip event, from @p item, or nothing when it is wrong.
  std::optional<EditorAnimationEvent> clipEvent(const EditorShellState& state,
                                                const json& item) {
    const auto sound = soundOf(state, item);
    const double at = agentNumberParam(item, "at").value_or(-1.0);
    if (!sound || at < 0.0) {
      return std::nullopt;
    }
    return EditorAnimationEvent{static_cast<float>(at), sound->first,
                                sound->second};
  }

  /// One sheet event, from @p item, or nothing when it is wrong.
  std::optional<EditorFrameEvent> frameEvent(const EditorShellState& state,
                                             const json& item) {
    const auto sound = soundOf(state, item);
    const std::optional<size_t> frame = agentIndexParam(item, "frame");
    if (!sound || !frame || *frame > UINT16_MAX) {
      return std::nullopt;
    }
    return EditorFrameEvent{static_cast<uint16_t>(*frame), sound->first,
                            sound->second};
  }

  /// Whether every one of @p events falls inside clip @p clip of @p asset,
  /// or its rig is not loaded yet to say.
  bool withinClip(const EditorAsset& asset, const std::string& clip,
                  const std::vector<EditorAnimationEvent>& events) {
    if (asset.rig == nullptr) {
      return true;
    }
    const auto found = std::ranges::find(asset.rig->clips, clip,
                                         &animation::AnimationClip::name);
    return found == asset.rig->clips.end() ||
           std::ranges::all_of(events, [&found](const auto& event) {
             return event.at <= found->duration;
           });
  }

  /// Write the row for clip @p clip of @p asset from the call's events —
  /// read in full before anything is replaced, so a refused call changes
  /// nothing — or take the row away when it gives none.
  AgentResult setClipRow(EditorShellState& state, const EditorAsset& asset,
                         const std::string& clip, const json& params) {
    const auto events = readEvents<EditorAnimationEvent>(
        params, [&state](const json& item) { return clipEvent(state, item); });
    if (params.contains("events") &&
        (!events || !withinClip(asset, clip, *events))) {
      return agentFailure(AgentStatus::BAD_PARAMS, SET_USAGE);
    }
    const std::string ref = editorAssetRef(asset);
    std::erase_if(state.animation_events.clips, [&](const auto& e) {
      return e.asset == ref && e.clip == clip;
    });
    if (events) {
      state.animation_events.clips.push_back({ref, clip, *events});
    }
    ++state.animation_events.revision;
    return agentOk(payload(state));
  }

  /// Write or take away the row for the sheet at @p sheet, as
  /// `setClipRow` does a clip's.
  AgentResult setSheetRow(EditorShellState& state, const std::string& sheet,
                          const json& params) {
    const auto events = readEvents<EditorFrameEvent>(
        params, [&state](const json& item) { return frameEvent(state, item); });
    if (params.contains("events") && !events) {
      return agentFailure(AgentStatus::BAD_PARAMS, SET_USAGE);
    }
    std::erase_if(state.animation_events.sheets,
                  [&](const auto& e) { return e.sheet == sheet; });
    if (events) {
      state.animation_events.sheets.push_back({sheet, *events});
    }
    ++state.animation_events.revision;
    return agentOk(payload(state));
  }

  /// The clip half of `set_animation_events`: the model and clip it names,
  /// and their row written.
  AgentResult setClip(EditorShellState& state, const json& params) {
    const std::string clip = agentStringParam(params, "clip").value_or("");
    const EditorAsset* asset =
        riggedAsset(state, agentStringParam(params, "asset").value_or(""));
    if (asset == nullptr || clip.empty() || !mayHaveClip(*asset, clip)) {
      return agentFailure(AgentStatus::NOT_FOUND,
                          "asset must be a rigged model list_assets lists, "
                          "and clip one of its clips");
    }
    return setClipRow(state, *asset, clip, params);
  }

  /// Whether the project holds a sheet at @p sheet.
  bool hasSheet(const EditorShellState& state, const std::string& sheet) {
    return std::ranges::any_of(state.sheets, [&sheet](const auto& path) {
      return path.generic_string() == sheet;
    });
  }

}  // namespace

AgentResult runAgentListAnimationEvents(EditorShellState& state,
                                        const json& /*params*/) {
  return agentOk(payload(state));
}

AgentResult runAgentSetAnimationEvents(EditorShellState& state,
                                       const json& params) {
  if (!state.project.loaded) {
    return agentFailure(AgentStatus::UNAVAILABLE, "no project is open");
  }
  if (const auto sheet = agentStringParam(params, "sheet")) {
    return hasSheet(state, *sheet)
               ? setSheetRow(state, *sheet, params)
               : agentFailure(AgentStatus::NOT_FOUND,
                              "sheet must be one list_sprites lists");
  }
  return setClip(state, params);
}

}  // namespace eng::editor
