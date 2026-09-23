// The Sound screen half of SimplishEditor: the user's volumes — applied to
// the client's audio and saved whenever they change — and the project's own
// sounds: which file each of the game's sounds plays, importing files to
// play, and hearing them. Kept apart, as the Controls screen is, because it
// edits settings and a project table rather than the level.

#include <algorithm>
#include <cmath>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-audio-volumes.h>
#include <editor/shell/editor-sound-import.h>
#include <editor/shell/editor-sound-ops.h>
#include <editor/shell/simplish-editor.h>
#include <engine/audio/audio-decode.h>
#include <engine/audio/audio-volumes-json.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/core/logger.h>
#include <engine/gui/gui-nav-buttons.h>
#include <span>
#include <utility>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

  /// How far one press of left or right turns a volume.
  constexpr float VOLUME_STEP = 0.05f;
  /// The bank name a previewed file is loaded under, replaced each time.
  constexpr std::string_view PREVIEW_CLIP = "editor.preview";

  /// @p level as the screen writes it: "80%".
  std::string percent(float level) {
    return std::to_string(static_cast<int>(std::lround(level * 100.0f))) + "%";
  }

  /// A volume row for @p name, keyed @p key, at @p level.
  EditorSoundRow volumeRow(std::string name, std::string key, float level) {
    return {.kind = EditorSoundRowKind::VOLUME,
            .name = std::move(name),
            .key = std::move(key),
            .value = percent(level),
            .level = level};
  }

  /// @p bus's name as the screen shows it: "Effects".
  std::string busLabel(audio::AudioBus bus) {
    std::string name{audio::audioBusName(bus)};
    name[0] = static_cast<char>(name[0] - 'a' + 'A');
    return name;
  }

  /// What a slot row says @p slot plays: the file, marked when the project
  /// no longer has it, or "Built-in".
  std::string slotValue(const EditorSoundTable& table,
                        std::span<const std::filesystem::path> files,
                        const std::string& slot) {
    const std::filesystem::path file = editorAssignedSound(table, slot);
    if (file.empty()) {
      return "Built-in";
    }
    const bool listed = std::ranges::find(files, file) != files.end();
    return file.generic_string() + (listed ? "" : " (missing)");
  }

  /// The volume @p key names in @p volumes: the master, or a bus's.
  float& volumeFor(audio::AudioVolumes& volumes, const std::string& key) {
    const std::optional<audio::AudioBus> bus = audio::audioBusNamed(key);
    return bus ? volumes.buses[static_cast<size_t>(*bus)] : volumes.master;
  }

}  // namespace

void SimplishEditor::initSound(GuiWidgetTree& tree) {
  auto sound = std::make_unique<EditorSoundWidget>();
  sound->on_dismissed = [this] {
    if (EditorSoundWidget* screen = soundWidget()) {
      screen->close();
    }
  };
  sound->on_level_picked = [this](size_t row, float level) {
    setSoundLevel(row, level);
  };
  sound_id_ = tree.insertExternalWidget(std::move(sound), root_panel_);
}

EditorSoundWidget* SimplishEditor::soundWidget() {
  return dynamic_cast<EditorSoundWidget*>(
      guiWidgetTree().findWidget(sound_id_));
}

void SimplishEditor::openSound() {
  EditorSoundWidget* screen = soundWidget();
  if (screen == nullptr) {
    return;
  }
  if (state_.playtest.mode == EditorPlayMode::CHOOSING) {
    showStatusMessage("Choose a character first");
    return;
  }
  screen->open(soundRows());
  showStatusMessage(state_.sound.file.empty()
                        ? "Sound — volumes last until the editor closes"
                        : "Sound — volumes saved to " +
                              state_.sound.file.string());
}

std::vector<EditorSoundRow> SimplishEditor::soundRows() const {
  std::vector<EditorSoundRow> rows;
  appendVolumeRows(rows);
  appendSlotRows(rows);
  return rows;
}

void SimplishEditor::appendVolumeRows(std::vector<EditorSoundRow>& rows) const {
  const audio::AudioVolumes& volumes = state_.sound.volumes;
  rows.push_back({.name = "Volume"});
  rows.push_back(volumeRow("Master", "master", volumes.master));
  for (uint8_t i = 0; i < audio::AUDIO_BUS_COUNT; ++i) {
    const auto bus = static_cast<audio::AudioBus>(i);
    rows.push_back(volumeRow(busLabel(bus),
                             std::string{audio::audioBusName(bus)},
                             audio::busVolume(volumes, bus)));
  }
  const bool muted = volumes.muting == audio::AudioMuting::MUTED;
  rows.push_back({.kind = EditorSoundRowKind::MUTE,
                  .name = "Mute",
                  .value = muted ? "On" : "Off"});
}

void SimplishEditor::appendSlotRows(std::vector<EditorSoundRow>& rows) const {
  if (!state_.project.loaded) {
    rows.push_back({.name = "Project sounds — open a project to add sounds"});
    return;
  }
  rows.push_back({.name = "Project sounds"});
  for (const std::string& slot : editorSoundSlots()) {
    rows.push_back(
        {.kind = EditorSoundRowKind::SLOT,
         .name = editorSoundSlotLabel(slot),
         .key = slot,
         .value = slotValue(state_.sounds, state_.sound_files, slot)});
  }
}

bool SimplishEditor::handleSoundKey(uint32_t key, ClientKeyDownKind kind) {
  EditorSoundWidget* screen = soundWidget();
  if (screen == nullptr || !screen->isOpen()) {
    return false;
  }
  if (key == Keycode::ARROW_UP || key == Keycode::ARROW_DOWN) {
    screen->moveHighlight(key == Keycode::ARROW_UP ? -1 : 1);
  } else if (key == Keycode::ARROW_LEFT || key == Keycode::ARROW_RIGHT) {
    stepSoundRow(key == Keycode::ARROW_LEFT ? -1 : 1);
  } else if (key == Keycode::ESCAPE) {
    screen->close();
  } else if (kind == ClientKeyDownKind::FIRST_PRESS) {
    handleSoundRowKey(key);
  }
  return true;
}

void SimplishEditor::handleSoundRowKey(uint32_t key) {
  if (key == Keycode::KEY_RETURN) {
    confirmSoundRow();
  } else if (key == Keycode::BACKSPACE || key == Keycode::DELETE_FORWARD) {
    clearSoundRow();
  } else if (key == 'm') {
    toggleMute();
  } else if (key == 'i') {
    importSound();
  }
}

bool SimplishEditor::handleSoundButton(input::GamepadButton button) {
  EditorSoundWidget* screen = soundWidget();
  if (screen == nullptr || !screen->isOpen()) {
    return false;
  }
  if (const std::optional<GuiNavCommand> command =
          guiNavCommandFor(button, gamepads().activeFamily())) {
    browseSound(*screen, *command);
  }
  return true;
}

void SimplishEditor::browseSound(EditorSoundWidget& screen,
                                 GuiNavCommand command) {
  if (command == GuiNavCommand::UP || command == GuiNavCommand::DOWN) {
    screen.moveHighlight(command == GuiNavCommand::UP ? -1 : 1);
  } else if (command == GuiNavCommand::LEFT ||
             command == GuiNavCommand::RIGHT) {
    stepSoundRow(command == GuiNavCommand::LEFT ? -1 : 1);
  } else if (command == GuiNavCommand::CONFIRM) {
    confirmSoundRow();
  } else if (command == GuiNavCommand::CANCEL) {
    screen.close();
  }
}

const EditorSoundRow* SimplishEditor::highlightedSoundRow() {
  const EditorSoundWidget* screen = soundWidget();
  if (screen == nullptr || !screen->isOpen()) {
    return nullptr;
  }
  return &screen->rows()[screen->highlighted()];
}

void SimplishEditor::stepSoundRow(int steps) {
  const EditorSoundRow* row = highlightedSoundRow();
  if (row == nullptr) {
    return;
  }
  if (row->kind == EditorSoundRowKind::VOLUME) {
    float& volume = volumeFor(state_.sound.volumes, row->key);
    volume = std::clamp(volume + VOLUME_STEP * static_cast<float>(steps), 0.0f,
                        1.0f);
    ++state_.sound.revision;
    return;
  }
  if (row->kind == EditorSoundRowKind::MUTE) {
    toggleMute();
  } else if (row->kind == EditorSoundRowKind::SLOT) {
    stepSoundSlot(row->key, steps);
  }
}

void SimplishEditor::stepSoundSlot(const std::string& slot, int steps) {
  assignEditorSound(
      state_.sounds, slot,
      editorStepSoundFile(editorAssignedSound(state_.sounds, slot),
                          state_.sound_files, steps));
  ++state_.sounds.revision;
}

void SimplishEditor::confirmSoundRow() {
  const EditorSoundRow* row = highlightedSoundRow();
  if (row == nullptr) {
    return;
  }
  if (row->kind == EditorSoundRowKind::MUTE) {
    toggleMute();
  } else if (row->kind == EditorSoundRowKind::SLOT) {
    (void)previewSound(row->key);
  }
}

void SimplishEditor::clearSoundRow() {
  const EditorSoundRow* row = highlightedSoundRow();
  if (row != nullptr && row->kind == EditorSoundRowKind::SLOT) {
    assignEditorSound(state_.sounds, row->key, {});
    ++state_.sounds.revision;
  }
}

void SimplishEditor::setSoundLevel(size_t row, float level) {
  const EditorSoundWidget* screen = soundWidget();
  if (screen == nullptr || row >= screen->rows().size() ||
      screen->rows()[row].kind != EditorSoundRowKind::VOLUME) {
    return;
  }
  volumeFor(state_.sound.volumes, screen->rows()[row].key) =
      std::clamp(level, 0.0f, 1.0f);
  ++state_.sound.revision;
}

void SimplishEditor::toggleMute() {
  audio::AudioMuting& muting = state_.sound.volumes.muting;
  muting = muting == audio::AudioMuting::MUTED ? audio::AudioMuting::AUDIBLE
                                               : audio::AudioMuting::MUTED;
  ++state_.sound.revision;
}

void SimplishEditor::importSound() {
  if (!state_.project.loaded) {
    showStatusMessage("Open a project to import sounds into");
    return;
  }
  const EditorSoundRow* row = highlightedSoundRow();
  import_sound_slot_ =
      row != nullptr && row->kind == EditorSoundRowKind::SLOT ? row->key : "";
  showOpenSoundDialog();
}

void SimplishEditor::onSoundFileChosen(const std::filesystem::path& path) {
  (void)importSoundFile(path, std::exchange(import_sound_slot_, {}));
}

EditorSoundImport
SimplishEditor::importSoundFile(const std::filesystem::path& source,
                                std::string_view slot) {
  if (!state_.project.loaded) {
    return {.error = "no project is open"};
  }
  EditorSoundImport imported =
      importEditorSound(projectAssetsPath(state_.project.root), source);
  if (!imported.error.empty()) {
    showStatusMessage("Not imported: " + imported.error);
    return imported;
  }
  rescanSoundFiles();
  assignImportedSound(slot, imported.file);
  showStatusMessage("Imported " + imported.file.generic_string() +
                    (slot.empty() ? "" : " for " + editorSoundSlotLabel(slot)));
  return imported;
}

void SimplishEditor::assignImportedSound(std::string_view slot,
                                         const std::filesystem::path& file) {
  if (!slot.empty()) {
    assignEditorSound(state_.sounds, slot, file);
    ++state_.sounds.revision;
  }
}

bool SimplishEditor::previewSound(std::string_view name) {
  if (const std::optional<std::string> slot = findEditorSoundSlot(name)) {
    const std::optional<audio::AudioClipId> clip = audio().clips().find(*slot);
    return clip && audio().play({.clip = *clip}).value != 0;
  }
  return state_.project.loaded && previewSoundFile(name);
}

bool SimplishEditor::previewSoundFile(std::string_view file) {
  std::optional<audio::AudioClip> clip = audio::loadAudioFile(
      projectAssetsPath(state_.project.root) / std::filesystem::path(file));
  if (!clip) {
    return false;
  }
  const audio::AudioClipId id =
      audio().clips().add(PREVIEW_CLIP, std::move(*clip));
  return audio().play({.clip = id}).value != 0;
}

void SimplishEditor::tickSound() {
  const bool volumes_moved = saved_sound_revision_ != state_.sound.revision;
  const bool table_moved = saved_sounds_revision_ != state_.sounds.revision;
  if (volumes_moved) {
    audio::applyAudioVolumes(audio(), state_.sound.volumes);
    if (saved_sound_revision_) {
      (void)saveEditorAudioVolumes(state_.sound.file, state_.sound.volumes);
    }
    saved_sound_revision_ = state_.sound.revision;
  }
  if (table_moved) {
    saveSoundTable();
  }
  if (EditorSoundWidget* screen = soundWidget();
      (volumes_moved || table_moved) && screen != nullptr && screen->isOpen()) {
    screen->refresh(soundRows());
  }
}

void SimplishEditor::saveSoundTable() {
  if (state_.project.loaded &&
      !saveEditorSoundTable(state_.project.root, state_.sounds)) {
    showStatusMessage("Could not save " +
                      editorSoundTablePath(state_.project.root).string());
    saved_sounds_revision_ = state_.sounds.revision;
    loadSoundClips();
    return;
  }
  reloadSounds();
}

void SimplishEditor::reloadSounds() {
  // Read back from the file, which every change is written to first, so
  // a hand edit and an edit here end the same way — and problems are the
  // file's as it now stands.
  const uint64_t revision = state_.sounds.revision;
  state_.sounds = state_.project.loaded
                      ? loadEditorSoundTable(state_.project.root)
                      : EditorSoundTable{};
  state_.sounds.revision = revision;
  saved_sounds_revision_ = revision;
  loadSoundClips();
}

void SimplishEditor::loadSoundClips() {
  const std::filesystem::path assets =
      state_.project.loaded ? projectAssetsPath(state_.project.root)
                            : std::filesystem::path{};
  EditorSoundLoad load = loadEditorSounds(audio().clips(), state_.sounds,
                                          assets, audio().sampleRate());
  combat_sounds_ = load.clips;
  state_.sounds.problems.insert(state_.sounds.problems.end(),
                                load.problems.begin(), load.problems.end());
  for (const std::string& problem : state_.sounds.problems) {
    LOG_WARN("editor", "sounds.data.json: " + problem);
  }
}

void SimplishEditor::rescanSoundFiles() {
  state_.sound_files =
      scanEditorAssets(projectAssetsPath(state_.project.root)).sounds;
}

}  // namespace eng::editor
