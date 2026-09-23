#include <algorithm>
#include <editor/shell/editor-animation-event-ops.h>
#include <editor/shell/editor-sound-ops.h>
#include <engine/animation/foot-contacts.h>
#include <engine/audio/audio-decode.h>

namespace eng::editor {

namespace {

  /// What a bank calls a file an event names.
  constexpr std::string_view FILE_PREFIX = "file:";

  /// The set clip @p clip of @p rig plays with no row of its own.
  EditorClipEventSet detected(const animation::Rig& rig, size_t clip) {
    EditorClipEventSet set;
    for (const float at : animation::detectFootContacts(rig, clip)) {
      set.events.push_back({at, std::string(EDITOR_FOOTSTEP_EVENT), 1.0F});
    }
    set.source = set.events.empty() ? EditorEventSource::NONE
                                    : EditorEventSource::DETECTED;
    return set;
  }

  /// @p table's row for clip @p clip of the model @p asset_ref, or null.
  const EditorClipEventEntry* rowFor(const EditorAnimationEventTable& table,
                                     std::string_view asset_ref,
                                     std::string_view clip) {
    const auto found = std::ranges::find_if(table.clips, [&](const auto& e) {
      return e.asset == asset_ref && e.clip == clip;
    });
    return found == table.clips.end() ? nullptr : &*found;
  }

  /// Every distinct file @p table's events name, in the order first named.
  std::vector<std::string> namedFiles(const EditorAnimationEventTable& table) {
    std::vector<std::string> files;
    const auto note = [&files](const std::string& sound) {
      if (editorEventNamesFile(sound) &&
          std::ranges::find(files, sound) == files.end()) {
        files.push_back(sound);
      }
    };
    for (const EditorClipEventEntry& entry : table.clips) {
      std::ranges::for_each(entry.events, note, &EditorAnimationEvent::sound);
    }
    for (const EditorSheetEventEntry& entry : table.sheets) {
      std::ranges::for_each(entry.events, note, &EditorFrameEvent::sound);
    }
    return files;
  }

}  // namespace

std::vector<EditorClipEventSet>
resolveEditorClipEvents(const EditorAnimationEventTable& table,
                        std::string_view asset_ref, const animation::Rig& rig) {
  std::vector<EditorClipEventSet> sets;
  for (size_t clip = 0; clip < rig.clips.size(); ++clip) {
    const EditorClipEventEntry* row =
        rowFor(table, asset_ref, rig.clips[clip].name);
    if (row == nullptr) {
      sets.push_back(detected(rig, clip));
      continue;
    }
    EditorClipEventSet set{row->events, EditorEventSource::AUTHORED};
    std::ranges::stable_sort(set.events, {}, &EditorAnimationEvent::at);
    sets.push_back(std::move(set));
  }
  return sets;
}

const EditorSheetEventEntry*
findEditorSheetEvents(const EditorAnimationEventTable& table,
                      std::string_view sheet) {
  const auto found =
      std::ranges::find(table.sheets, sheet, &EditorSheetEventEntry::sheet);
  return found == table.sheets.end() ? nullptr : &*found;
}

bool editorEventsStep(const EditorClipEventSet& set) {
  return std::ranges::any_of(set.events, [](const EditorAnimationEvent& e) {
    return e.sound == EDITOR_FOOTSTEP_EVENT;
  });
}

bool editorEventNamesFile(std::string_view sound) {
  return sound != EDITOR_FOOTSTEP_EVENT && !findEditorSoundSlot(sound);
}

std::vector<std::string>
loadEditorEventSounds(audio::AudioClipBank& bank,
                      const EditorAnimationEventTable& table,
                      const std::filesystem::path& assets_dir) {
  std::vector<std::string> problems;
  for (const std::string& file : namedFiles(table)) {
    std::optional<audio::AudioClip> clip =
        audio::loadAudioFile(assets_dir / file);
    if (clip) {
      (void)bank.add(std::string(FILE_PREFIX) + file, std::move(*clip));
    } else {
      problems.push_back(file + ": missing, or not a WAV or Ogg Vorbis file");
    }
  }
  return problems;
}

std::optional<audio::AudioClipId>
findEditorEventClip(const audio::AudioClipBank& bank, std::string_view sound) {
  if (const std::optional<std::string> slot = findEditorSoundSlot(sound)) {
    return bank.find(*slot);
  }
  return bank.find(std::string(FILE_PREFIX) + std::string(sound));
}

}  // namespace eng::editor
