// The Controls screen half of SimplishEditor: opening it, routing keys and
// pad presses to it while it is open, rebinding what it listens for, and
// saving the controls whenever they change — from here or from an agent's
// set_controls. Kept apart from simplish-editor.cpp because it is the one
// modal screen that edits the user's settings rather than the level.

#include <cmath>
#include <editor/shell/editor-controls-ops.h>
#include <editor/shell/editor-input-bindings.h>
#include <editor/shell/simplish-editor.h>
#include <engine/client/desktop-platform-keycode.h>
#include <engine/gui/gui-nav-buttons.h>

namespace eng::editor {

namespace {

  using Keycode = eng::client::DesktopPlatformKeycode;

  /// How far a stick or trigger must go, while listening, to be what is
  /// bound: well past drift, so a pad lying on the desk binds nothing.
  constexpr float LISTEN_THRESHOLD = 0.6f;

  /// The stick or trigger direction @p pad is pushing past the threshold,
  /// the first found, or nothing.
  std::optional<input::InputSource> pushedAxis(const input::GamepadState& pad) {
    for (size_t i = 0; i < input::GAMEPAD_AXIS_COUNT; ++i) {
      const auto axis = static_cast<input::GamepadAxis>(i);
      const float value = pad.axis(axis);
      if (std::abs(value) >= LISTEN_THRESHOLD) {
        return value > 0.0f ? input::InputSource::positive(axis)
                            : input::InputSource::negative(axis);
      }
    }
    return std::nullopt;
  }

}  // namespace

void SimplishEditor::initControls(GuiWidgetTree& tree) {
  auto controls = std::make_unique<EditorControlsWidget>();
  controls->on_dismissed = [this] {
    if (EditorControlsWidget* screen = controlsWidget()) {
      screen->close();
    }
  };
  controls_id_ = tree.insertExternalWidget(std::move(controls), stage_panel_);
  coverStage(tree, controls_id_);
}

EditorControlsWidget* SimplishEditor::controlsWidget() {
  return dynamic_cast<EditorControlsWidget*>(
      guiWidgetTree().findWidget(controls_id_));
}

void SimplishEditor::openControls() {
  EditorControlsWidget* screen = controlsWidget();
  if (screen == nullptr) {
    return;
  }
  if (isPlaying() || state_.playtest.mode == EditorPlayMode::CHOOSING) {
    showStatusMessage("Stop playing to change the controls");
    return;
  }
  screen->open(controlsRows());
  showStatusMessage(state_.controls.file.empty()
                        ? "Controls — changes last until the editor closes"
                        : "Controls — saved to " +
                              state_.controls.file.string());
}

std::vector<EditorControlsRow> SimplishEditor::controlsRows() const {
  const input::GamepadFamily family = gamepads().activeFamily();
  const input::InputBindings& bindings = state_.controls.bindings;
  std::vector<EditorControlsRow> rows;
  for (size_t i = 0; i < input::INPUT_ACTION_COUNT; ++i) {
    const auto action = static_cast<input::InputAction>(i);
    rows.push_back({action, std::string{editorActionLabel(action)},
                    editorActionKeysLabel(bindings, action),
                    editorActionControlsLabel(bindings, action, family)});
  }
  return rows;
}

bool SimplishEditor::handleControlsKey(uint32_t key, ClientKeyDownKind kind) {
  EditorControlsWidget* screen = controlsWidget();
  if (screen == nullptr || !screen->isOpen()) {
    return false;
  }
  if (screen->mode() == EditorControlsMode::BROWSING) {
    handleControlsBrowseKey(key);
  } else if (key == Keycode::ESCAPE) {
    screen->stopListening();
  } else if (kind == ClientKeyDownKind::FIRST_PRESS) {
    bindListened(input::InputSource::key(key));
  }
  return true;
}

void SimplishEditor::handleControlsBrowseKey(uint32_t key) {
  EditorControlsWidget& screen = *controlsWidget();
  const input::InputAction action = screen.rows()[screen.highlighted()].action;
  if (key == Keycode::ARROW_UP || key == Keycode::ARROW_DOWN) {
    screen.moveHighlight(key == Keycode::ARROW_UP ? -1 : 1);
  } else if (key == Keycode::KEY_RETURN) {
    screen.listen();
  } else if (key == Keycode::BACKSPACE || key == Keycode::DELETE_FORWARD) {
    state_.controls.bindings.clear(action);
    controlsChanged();
  } else if (key == 'r') {
    resetControls();
  } else if (key == Keycode::ESCAPE) {
    screen.close();
  }
}

bool SimplishEditor::handleControlsButton(input::GamepadButton button) {
  EditorControlsWidget* screen = controlsWidget();
  if (screen == nullptr || !screen->isOpen()) {
    return false;
  }
  if (screen->mode() == EditorControlsMode::LISTENING) {
    bindListened(input::InputSource::button(button));
  } else if (const std::optional<GuiNavCommand> command =
                 guiNavCommandFor(button, gamepads().activeFamily())) {
    // Browsing, the pad moves and picks as it does in any menu on it.
    browseControls(*screen, *command);
  }
  return true;
}

void SimplishEditor::browseControls(EditorControlsWidget& screen,
                                    GuiNavCommand command) {
  if (command == GuiNavCommand::UP || command == GuiNavCommand::DOWN) {
    screen.moveHighlight(command == GuiNavCommand::UP ? -1 : 1);
  } else if (command == GuiNavCommand::CONFIRM) {
    screen.listen();
  } else if (command == GuiNavCommand::CANCEL) {
    screen.close();
  }
}

void SimplishEditor::resetControls() {
  const input::GamepadDeadzones kept = state_.controls.bindings.deadzones();
  state_.controls.bindings = editorDefaultInputBindings();
  state_.controls.bindings.setDeadzones(kept);
  controlsChanged();
}

void SimplishEditor::bindListened(input::InputSource source) {
  EditorControlsWidget& screen = *controlsWidget();
  editorRebind(state_.controls.bindings,
               screen.rows()[screen.highlighted()].action, source);
  controlsChanged();
}

void SimplishEditor::controlsChanged() {
  ++state_.controls.revision;
}

void SimplishEditor::tickControls() {
  // Seated first, so a pad picked up this frame can join as a player.
  seats_.update(gamepads());
  listenForAxis();
  if (state_.controls.revision == saved_controls_revision_) {
    return;
  }
  // Changed here or by an agent: show it, and keep it.
  if (EditorControlsWidget* screen = controlsWidget();
      screen != nullptr && screen->isOpen()) {
    screen->refresh(controlsRows());
  }
  (void)saveEditorInputBindings(state_.controls.file, state_.controls.bindings);
  saved_controls_revision_ = state_.controls.revision;
}

void SimplishEditor::listenForAxis() {
  const EditorControlsWidget* screen = controlsWidget();
  const input::GamepadState* pad = gamepads().active();
  if (screen == nullptr || !screen->isOpen() || pad == nullptr ||
      screen->mode() != EditorControlsMode::LISTENING) {
    return;
  }
  if (const std::optional<input::InputSource> axis = pushedAxis(*pad)) {
    bindListened(*axis);
  }
}

}  // namespace eng::editor
