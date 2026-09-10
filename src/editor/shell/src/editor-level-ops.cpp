#include <algorithm>
#include <editor/shell/editor-action-ops.h>
#include <editor/shell/editor-level-io.h>
#include <editor/shell/editor-level-list.h>
#include <editor/shell/editor-level-load.h>
#include <editor/shell/editor-level-ops.h>
#include <optional>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  bool isIdLetter(char c) {
    return c >= 'a' && c <= 'z';
  }

  bool isIdChar(char c) {
    return isIdLetter(c) || (c >= '0' && c <= '9') || c == '_';
  }

  /// What @p c contributes to an id: itself, an underscore for a
  /// separator, or nothing at all.
  char idCharFrom(char c) {
    const char lower =
        c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    if (isIdChar(lower)) {
      return lower;
    }
    return c == ' ' || c == '-' || c == '.' || c == '/' ? '_' : '\0';
  }

  /// Whether @p mapped may follow what @p id holds so far. An id starts
  /// with a letter and never carries two underscores in a row.
  bool acceptsIdChar(const std::string& id, char mapped) {
    if (id.empty()) {
      return isIdLetter(mapped);
    }
    return mapped != '_' || id.back() != '_';
  }

  /// Read the open level's file into @p state, if it has one.
  EditorLevelResult readOpenLevel(EditorShellState& state) {
    if (!editorLevelExists(state)) {
      return {};
    }
    std::optional<EditorLevelLoad> load = loadEditorLevel(state);
    if (!load) {
      // Remembered rather than only reported: this is what stops the next
      // save from writing an empty level over the file that would not parse.
      state.level_readable = false;
      return {EditorLevelStatus::UNREADABLE, 0};
    }
    state.document = std::move(load->document);
    return {EditorLevelStatus::OK, load->dropped_props, load->dropped_entities};
  }

  /// Make @p id the open level, and read whatever is in its file.
  EditorLevelResult adoptLevel(EditorShellState& state, std::string_view id) {
    state.level_id = std::string(id);
    state.document = EditorDocument{};
    state.selection = EditorSelection{};
    clearEditorActions(state.history);
    markEditorChangesSaved(state.history);
    state.level_readable = true;
    refreshEditorLevels(state);
    return readOpenLevel(state);
  }

  /// What stops a switch to @p id before anything is read or written.
  EditorLevelStatus levelSwitchGuard(const EditorShellState& state,
                                     std::string_view id,
                                     EditorLevelUnsaved unsaved) {
    if (!state.project.loaded) {
      return EditorLevelStatus::NO_PROJECT;
    }
    if (!isEditorLevelId(id)) {
      return EditorLevelStatus::INVALID_ID;
    }
    if (id != state.level_id && unsaved == EditorLevelUnsaved::REFUSE &&
        hasUnsavedEditorChanges(state.history)) {
      return EditorLevelStatus::UNSAVED_CHANGES;
    }
    return EditorLevelStatus::OK;
  }

  /// The first level of @p levels with a file behind it.
  std::optional<std::string>
  firstLevelOnDisk(const std::vector<EditorLevelEntry>& levels) {
    const auto found =
        std::find_if(levels.begin(), levels.end(),
                     [](const EditorLevelEntry& e) { return e.on_disk; });
    if (found == levels.end()) {
      return std::nullopt;
    }
    return found->id;
  }

}  // namespace

bool isEditorLevelId(std::string_view id) {
  if (id.empty() || id.size() > EDITOR_LEVEL_ID_MAX ||
      !isIdLetter(id.front())) {
    return false;
  }
  return std::all_of(id.begin(), id.end(), [](char c) { return isIdChar(c); });
}

std::string editorLevelIdFromText(std::string_view text) {
  std::string id;
  for (char c : text) {
    const char mapped = idCharFrom(c);
    if (mapped != '\0' && acceptsIdChar(id, mapped)) {
      id.push_back(mapped);
    }
  }
  while (!id.empty() && id.back() == '_') {
    id.pop_back();
  }
  return id.size() > EDITOR_LEVEL_ID_MAX ? id.substr(0, EDITOR_LEVEL_ID_MAX)
                                         : id;
}

EditorLevelStatus canCreateEditorLevel(const EditorShellState& state,
                                       std::string_view id,
                                       EditorLevelUnsaved unsaved) {
  const EditorLevelStatus guard = levelSwitchGuard(state, id, unsaved);
  if (guard != EditorLevelStatus::OK) {
    return guard;
  }
  return hasEditorLevel(state, id) ? EditorLevelStatus::ALREADY_EXISTS
                                   : EditorLevelStatus::OK;
}

EditorLevelStatus canOpenEditorLevel(const EditorShellState& state,
                                     std::string_view id,
                                     EditorLevelUnsaved unsaved) {
  const EditorLevelStatus guard = levelSwitchGuard(state, id, unsaved);
  if (guard != EditorLevelStatus::OK) {
    return guard;
  }
  return hasEditorLevel(state, id) ? EditorLevelStatus::OK
                                   : EditorLevelStatus::NOT_FOUND;
}

EditorLevelResult createEditorLevel(EditorShellState& state,
                                    std::string_view id,
                                    EditorLevelUnsaved unsaved) {
  const EditorLevelStatus guard = canCreateEditorLevel(state, id, unsaved);
  if (guard != EditorLevelStatus::OK) {
    return {guard, 0};
  }
  if (!createEditorLevelFile(state.project.root, id)) {
    return {EditorLevelStatus::WRITE_FAILED, 0};
  }
  return adoptLevel(state, id);
}

EditorLevelResult openEditorLevel(EditorShellState& state, std::string_view id,
                                  EditorLevelUnsaved unsaved) {
  const EditorLevelStatus guard = canOpenEditorLevel(state, id, unsaved);
  if (guard != EditorLevelStatus::OK) {
    return {guard, 0};
  }
  return adoptLevel(state, id);
}

void chooseEditorStartLevel(EditorShellState& state) {
  state.level_id = std::string(EDITOR_LEVEL_ID);
  refreshEditorLevels(state);
  // `main` when the project has one. Otherwise the first level it does
  // have, because a project whose levels are named something else should
  // open on one of them rather than on an empty level it does not hold.
  if (editorLevelExists(state)) {
    return;
  }
  if (const std::optional<std::string> first = firstLevelOnDisk(state.levels)) {
    state.level_id = *first;
    refreshEditorLevels(state);
  }
}

}  // namespace eng::editor
