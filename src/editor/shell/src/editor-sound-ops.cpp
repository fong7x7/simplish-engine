#include <algorithm>
#include <array>
#include <editor/shell/editor-sound-ops.h>
#include <engine/audio/audio-decode.h>

namespace eng::editor {

namespace {

  /// What the Sound screen calls each combat cue's slot, by kind.
  constexpr std::array<std::string_view, game::COMBAT_CUE_KIND_COUNT>
      COMBAT_LABELS{"Shot fired", "Shot hits someone", "Shot hits a wall",
                    "Blast"};

  /// The combat cue whose slot @p slot is, if it is one.
  std::optional<game::CombatCueKind> combatKindOf(std::string_view slot) {
    for (uint8_t i = 0; i < game::COMBAT_CUE_KIND_COUNT; ++i) {
      const auto kind = static_cast<game::CombatCueKind>(i);
      if (game::combatSoundName(kind) == slot) {
        return kind;
      }
    }
    return std::nullopt;
  }

  /// Load @p entry's file from under @p assets_dir into @p bank over its
  /// slot, or say why not.
  void loadEntry(audio::AudioClipBank& bank, const EditorSoundEntry& entry,
                 const std::filesystem::path& assets_dir,
                 EditorSoundLoad& out) {
    if (!combatKindOf(entry.slot)) {
      out.problems.push_back(entry.slot + ": no such sound");
      return;
    }
    std::optional<audio::AudioClip> clip =
        audio::loadAudioFile(assets_dir / entry.file);
    if (!clip) {
      out.problems.push_back(entry.slot + ": " + entry.file.generic_string() +
                             " is missing or not a WAV or Ogg Vorbis file");
      return;
    }
    (void)bank.add(entry.slot, std::move(*clip));
  }

}  // namespace

std::vector<std::string> editorSoundSlots() {
  std::vector<std::string> slots;
  slots.reserve(game::COMBAT_CUE_KIND_COUNT);
  for (uint8_t i = 0; i < game::COMBAT_CUE_KIND_COUNT; ++i) {
    slots.emplace_back(
        game::combatSoundName(static_cast<game::CombatCueKind>(i)));
  }
  return slots;
}

std::string editorSoundSlotLabel(std::string_view slot) {
  const std::optional<game::CombatCueKind> kind = combatKindOf(slot);
  return std::string{kind ? COMBAT_LABELS[static_cast<size_t>(*kind)] : slot};
}

std::optional<std::string> findEditorSoundSlot(std::string_view name) {
  for (const std::string& slot : editorSoundSlots()) {
    const std::string_view cue =
        std::string_view{slot}.substr(slot.find('.') + 1);
    if (name == slot || name == cue) {
      return slot;
    }
  }
  return std::nullopt;
}

std::filesystem::path editorAssignedSound(const EditorSoundTable& table,
                                          std::string_view slot) {
  const auto found =
      std::ranges::find(table.sounds, slot, &EditorSoundEntry::slot);
  return found == table.sounds.end() ? std::filesystem::path{} : found->file;
}

void assignEditorSound(EditorSoundTable& table, std::string_view slot,
                       const std::filesystem::path& file) {
  const auto found =
      std::ranges::find(table.sounds, slot, &EditorSoundEntry::slot);
  if (file.empty()) {
    if (found != table.sounds.end()) {
      table.sounds.erase(found);
    }
  } else if (found != table.sounds.end()) {
    found->file = file;
  } else {
    table.sounds.push_back({std::string{slot}, file});
  }
}

std::filesystem::path
editorStepSoundFile(const std::filesystem::path& current,
                    std::span<const std::filesystem::path> files, int steps) {
  const auto count = static_cast<long>(files.size()) + 1;
  const auto found = std::ranges::find(files, current);
  const long at = found == files.end() || current.empty()
                      ? 0
                      : static_cast<long>(found - files.begin()) + 1;
  const long to = ((at + steps) % count + count) % count;
  return to == 0 ? std::filesystem::path{} : files[static_cast<size_t>(to - 1)];
}

EditorSoundLoad loadEditorSounds(audio::AudioClipBank& bank,
                                 const EditorSoundTable& table,
                                 const std::filesystem::path& assets_dir,
                                 uint32_t sample_rate) {
  EditorSoundLoad out{.clips = game::loadCombatSounds(bank, sample_rate)};
  for (const EditorSoundEntry& entry : table.sounds) {
    loadEntry(bank, entry, assets_dir, out);
  }
  return out;
}

}  // namespace eng::editor
