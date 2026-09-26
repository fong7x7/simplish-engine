#include <algorithm>
#include <array>
#include <cctype>
#include <editor/shell/editor-controls-ops.h>
#include <engine/client/desktop-key-names.h>
#include <engine/input/gamepad-button-label.h>
#include <engine/input/input-bindings-json.h>
#include <vector>

namespace eng::editor {

namespace {

  /// Each action's label, in `InputAction` order.
  constexpr std::array<std::string_view, input::INPUT_ACTION_COUNT>
      ACTION_LABELS = {"Move up",   "Move down", "Move left", "Move right",
                       "Fire",      "Aim up",    "Aim down",  "Aim left",
                       "Aim right", "Pause"};

  /// What a list of no controls shows.
  constexpr std::string_view NONE = "—";

  /// @p word with its first letter capitalised: "up" to "Up", "w" to "W".
  std::string capitalised(std::string word) {
    if (!word.empty()) {
      word[0] =
          static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
    }
    return word;
  }

  /// A key's label: its file name after "key:", capitalised.
  std::string keyLabel(input::InputSource source) {
    const std::string text =
        input::inputSourceText(source, client::desktopKeyNames());
    return capitalised(text.substr(text.find(':') + 1));
  }

  /// The direction word a stick axis pushed @p source's way is.
  std::string_view stickDirection(input::InputSource source) {
    const bool negative =
        source.kind == input::InputSourceKind::GAMEPAD_AXIS_NEGATIVE;
    const auto axis = static_cast<input::GamepadAxis>(source.code);
    const bool vertical = axis == input::GamepadAxis::LEFT_Y ||
                          axis == input::GamepadAxis::RIGHT_Y;
    if (vertical) {
      return negative ? " Up" : " Down";
    }
    return negative ? " Left" : " Right";
  }

  /// An axis direction's label: the stick and which way, or the trigger.
  std::string axisLabel(input::InputSource source,
                        input::GamepadFamily family) {
    const auto axis = static_cast<input::GamepadAxis>(source.code);
    const std::string name{input::gamepadAxisLabel(axis, family)};
    const bool trigger = axis == input::GamepadAxis::LEFT_TRIGGER ||
                         axis == input::GamepadAxis::RIGHT_TRIGGER;
    return trigger ? name : name + std::string{stickDirection(source)};
  }

  /// Which device's controls a label lists.
  enum class ControlSide : uint8_t {
    /// The keyboard's.
    KEYS,
    /// A pad's.
    PAD,
  };

  /// Every control on @p action on @p side, labelled for @p family, joined.
  std::string joinedLabels(const input::InputBindings& bindings,
                           input::InputAction action, ControlSide side,
                           input::GamepadFamily family) {
    std::string out;
    for (const input::InputSource source : bindings.sources(action)) {
      if (editorIsPadControl(source) == (side == ControlSide::PAD)) {
        out += (out.empty() ? "" : " / ") + editorControlLabel(source, family);
      }
    }
    return out.empty() ? std::string{NONE} : out;
  }

}  // namespace

bool editorIsPadControl(input::InputSource source) {
  return source.kind != input::InputSourceKind::KEY;
}

void editorRebind(input::InputBindings& bindings, input::InputAction action,
                  input::InputSource source) {
  const std::vector<input::InputSource> was{bindings.sources(action).begin(),
                                            bindings.sources(action).end()};
  for (const input::InputSource old : was) {
    if (editorIsPadControl(old) == editorIsPadControl(source)) {
      bindings.unbind(action, old);
    }
  }
  bindings.bind(action, source);
}

std::string_view editorActionLabel(input::InputAction action) {
  const auto index = static_cast<size_t>(action);
  return index < ACTION_LABELS.size() ? ACTION_LABELS[index] : NONE;
}

std::string editorControlLabel(input::InputSource source,
                               input::GamepadFamily family) {
  switch (source.kind) {
    case input::InputSourceKind::KEY:
      return keyLabel(source);
    case input::InputSourceKind::GAMEPAD_BUTTON:
      return std::string{input::gamepadButtonLabel(
          static_cast<input::GamepadButton>(source.code), family)};
    case input::InputSourceKind::GAMEPAD_AXIS_POSITIVE:
    case input::InputSourceKind::GAMEPAD_AXIS_NEGATIVE:
      break;
  }
  return axisLabel(source, family);
}

std::string editorActionControlsLabel(const input::InputBindings& bindings,
                                      input::InputAction action,
                                      input::GamepadFamily pad_family) {
  return joinedLabels(bindings, action, ControlSide::PAD, pad_family);
}

std::string editorActionKeysLabel(const input::InputBindings& bindings,
                                  input::InputAction action) {
  return joinedLabels(bindings, action, ControlSide::KEYS,
                      input::GamepadFamily::GENERIC);
}

}  // namespace eng::editor
