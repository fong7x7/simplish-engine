#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <engine/input/input-bindings-json.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <utility>

namespace eng::input {

namespace {

  /// The format this writes, and the only one it reads.
  constexpr int FORMAT_VERSION = 1;

  /// Each `InputAction`'s name in the file, in enum order.
  constexpr std::array<std::string_view, INPUT_ACTION_COUNT> ACTION_NAMES = {
      "move_up", "move_down", "move_left", "move_right", "fire",
      "aim_up",  "aim_down",  "aim_left",  "aim_right",  "pause",
  };

  /// Each `GamepadButton`'s name in the file, in enum order.
  constexpr std::array<std::string_view, GAMEPAD_BUTTON_COUNT> BUTTON_NAMES = {
      "south",          "east",          "west",           "north",
      "back",           "guide",         "start",          "left_stick",
      "right_stick",    "left_shoulder", "right_shoulder", "dpad_up",
      "dpad_down",      "dpad_left",     "dpad_right",     "misc",
      "right_paddle_1", "left_paddle_1", "right_paddle_2", "left_paddle_2",
      "touchpad",
  };

  /// Each `GamepadAxis`'s name in the file, in enum order.
  constexpr std::array<std::string_view, GAMEPAD_AXIS_COUNT> AXIS_NAMES = {
      "left_x", "left_y", "right_x", "right_y", "left_trigger", "right_trigger",
  };

  /// What a source string starts with, per device.
  constexpr std::string_view KEY_PREFIX = "key:";
  constexpr std::string_view PAD_PREFIX = "pad:";

  /// The printable ASCII range a key is written as its own character in.
  constexpr uint32_t PRINTABLE_FIRST = 0x21U;
  constexpr uint32_t PRINTABLE_LAST = 0x7EU;

  /// The index of @p name in @p names, or nothing.
  template <std::size_t N>
  std::optional<uint32_t> indexOf(const std::array<std::string_view, N>& names,
                                  std::string_view name) {
    const auto found = std::ranges::find(names, name);
    return found == names.end()
               ? std::nullopt
               : std::optional{static_cast<uint32_t>(found - names.begin())};
  }

  /// @p key as the file names it.
  std::string keyText(uint32_t key, std::span<const KeyName> keys) {
    const auto named = std::ranges::find(keys, key, &KeyName::key);
    if (named != keys.end()) {
      return std::string{named->name};
    }
    if (key >= PRINTABLE_FIRST && key <= PRINTABLE_LAST) {
      return {static_cast<char>(key)};
    }
    std::array<char, 16> hex{};
    (void)std::snprintf(hex.data(), hex.size(), "0x%X", key);
    return hex.data();
  }

  /// A pad axis direction as the file names it: sticks signed, triggers,
  /// which only go one way, bare.
  std::string axisText(InputSource source) {
    const auto axis = static_cast<GamepadAxis>(source.code);
    const bool trigger =
        axis == GamepadAxis::LEFT_TRIGGER || axis == GamepadAxis::RIGHT_TRIGGER;
    const bool negative = source.kind == InputSourceKind::GAMEPAD_AXIS_NEGATIVE;
    const std::string_view sign = negative ? "-" : (trigger ? "" : "+");
    return std::string{PAD_PREFIX} + std::string{sign} +
           std::string{AXIS_NAMES[source.code]};
  }

  /// The key symbol @p text spells in hexadecimal — `0x40000052` — or
  /// nothing.
  std::optional<uint32_t> parseHexKey(std::string_view text) {
    if (!text.starts_with("0x") && !text.starts_with("0X")) {
      return std::nullopt;
    }
    uint32_t code = 0;
    const char* last = text.data() + text.size();
    const auto result = std::from_chars(text.data() + 2, last, code, 16);
    return result.ec == std::errc{} && result.ptr == last && text.size() > 2
               ? std::optional{code}
               : std::nullopt;
  }

  /// Whether @p text is one printable character, which names its own key.
  bool isPrintableChar(std::string_view text) {
    const auto c = text.empty() ? 0U : static_cast<uint8_t>(text[0]);
    return text.size() == 1 && c >= PRINTABLE_FIRST && c <= PRINTABLE_LAST;
  }

  /// The key @p text names, or nothing.
  std::optional<uint32_t> parseKey(std::string_view text,
                                   std::span<const KeyName> keys) {
    const auto named = std::ranges::find(keys, text, &KeyName::name);
    if (named != keys.end()) {
      return named->key;
    }
    if (isPrintableChar(text)) {
      return static_cast<uint32_t>(static_cast<uint8_t>(text[0]));
    }
    return parseHexKey(text);
  }

  /// The pad axis direction @p text names — `+left_x`, `-right_y`, or a
  /// bare name meaning the positive way — or nothing.
  std::optional<InputSource> parseAxis(std::string_view text) {
    InputSourceKind kind = InputSourceKind::GAMEPAD_AXIS_POSITIVE;
    if (text.starts_with('-')) {
      kind = InputSourceKind::GAMEPAD_AXIS_NEGATIVE;
    }
    if (text.starts_with('-') || text.starts_with('+')) {
      text.remove_prefix(1);
    }
    const std::optional<uint32_t> axis = indexOf(AXIS_NAMES, text);
    return axis ? std::optional{InputSource{kind, *axis}} : std::nullopt;
  }

  /// The pad button or axis @p text names, or nothing.
  std::optional<InputSource> parsePad(std::string_view text) {
    if (const std::optional<uint32_t> button = indexOf(BUTTON_NAMES, text)) {
      return InputSource{InputSourceKind::GAMEPAD_BUTTON, *button};
    }
    return parseAxis(text);
  }

  /// Where a parse is going: the scheme so far, its problems, and the key
  /// names to read keys by.
  struct Reader {
    /// The scheme and problems read so far.
    InputBindingsLoad& out;
    /// The platform's key names.
    std::span<const KeyName> keys;
  };

  /// Bind the control @p entry names to @p action named @p name, or
  /// report it.
  void readEntry(Reader& reader, InputAction action,
                 const nlohmann::json& entry) {
    const std::optional<InputSource> source =
        entry.is_string()
            ? parseInputSource(entry.get_ref<const std::string&>(), reader.keys)
            : std::nullopt;
    if (source) {
      reader.out.bindings.bind(action, *source);
      return;
    }
    const std::string_view name =
        ACTION_NAMES[static_cast<std::size_t>(action)];
    reader.out.problems.push_back(std::string{name} + ": skipped " +
                                  entry.dump());
  }

  /// Read @p json, one action's list of controls, into @p action.
  void readAction(Reader& reader, InputAction action,
                  const nlohmann::json& json) {
    if (!json.is_array()) {
      const std::string_view name =
          ACTION_NAMES[static_cast<std::size_t>(action)];
      reader.out.problems.push_back(std::string{name} + " is not a list");
      return;
    }
    reader.out.bindings.clear(action);
    for (const nlohmann::json& entry : json) {
      readEntry(reader, action, entry);
    }
  }

  /// Read the `actions` object @p json into @p reader.
  void readActions(Reader& reader, const nlohmann::json& json) {
    if (!json.is_object()) {
      reader.out.problems.emplace_back("actions is not an object");
      return;
    }
    for (const auto& [name, value] : json.items()) {
      if (const std::optional<InputAction> action = inputActionNamed(name)) {
        readAction(reader, *action, value);
      } else {
        reader.out.problems.push_back("no action called " + name);
      }
    }
  }

  /// @p json's number field @p name, or @p fallback when it has none.
  float numberOr(const nlohmann::json& json, const char* name, float fallback) {
    const auto found = json.find(name);
    return found != json.end() && found->is_number() ? found->get<float>()
                                                     : fallback;
  }

  /// Read the `deadzones` object @p json into @p reader.
  void readDeadzones(Reader& reader, const nlohmann::json& json) {
    if (!json.is_object()) {
      reader.out.problems.emplace_back("deadzones is not an object");
      return;
    }
    const GamepadDeadzones& was = reader.out.bindings.deadzones();
    reader.out.bindings.setDeadzones(
        {numberOr(json, "left_stick", was.left_stick),
         numberOr(json, "right_stick", was.right_stick),
         numberOr(json, "trigger", was.trigger)});
  }

  /// Read the whole document @p root into @p reader.
  void readRoot(Reader& reader, const nlohmann::json& root) {
    const auto version = root.find("version");
    if (version != root.end() &&
        !(version->is_number_integer() && *version == FORMAT_VERSION)) {
      reader.out.problems.push_back("version is not " +
                                    std::to_string(FORMAT_VERSION));
    }
    if (const auto deadzones = root.find("deadzones");
        deadzones != root.end()) {
      readDeadzones(reader, *deadzones);
    }
    if (const auto actions = root.find("actions"); actions != root.end()) {
      readActions(reader, *actions);
    }
  }

  /// @p value as the file writes it: to three places, so a player reads
  /// 0.2 rather than the float's 0.20000000298023224.
  double tidy(float value) {
    constexpr double PLACES = 1000.0;
    return std::round(static_cast<double>(value) * PLACES) / PLACES;
  }

  /// @p zones as the file's `deadzones` object.
  nlohmann::ordered_json deadzonesJson(const GamepadDeadzones& zones) {
    return {{"left_stick", tidy(zones.left_stick)},
            {"right_stick", tidy(zones.right_stick)},
            {"trigger", tidy(zones.trigger)}};
  }

}  // namespace

std::string_view inputActionName(InputAction action) {
  const auto index = static_cast<std::size_t>(action);
  return index < ACTION_NAMES.size() ? ACTION_NAMES[index] : std::string_view{};
}

std::optional<InputAction> inputActionNamed(std::string_view name) {
  const std::optional<uint32_t> index = indexOf(ACTION_NAMES, name);
  return index ? std::optional{static_cast<InputAction>(*index)} : std::nullopt;
}

std::string inputSourceText(InputSource source, std::span<const KeyName> keys) {
  switch (source.kind) {
    case InputSourceKind::KEY:
      return std::string{KEY_PREFIX} + keyText(source.code, keys);
    case InputSourceKind::GAMEPAD_BUTTON:
      return std::string{PAD_PREFIX} + std::string{BUTTON_NAMES[source.code]};
    case InputSourceKind::GAMEPAD_AXIS_POSITIVE:
    case InputSourceKind::GAMEPAD_AXIS_NEGATIVE:
      break;
  }
  return axisText(source);
}

std::optional<InputSource> parseInputSource(std::string_view text,
                                            std::span<const KeyName> keys) {
  if (text.starts_with(KEY_PREFIX)) {
    const auto key = parseKey(text.substr(KEY_PREFIX.size()), keys);
    return key ? std::optional{InputSource::key(*key)} : std::nullopt;
  }
  if (text.starts_with(PAD_PREFIX)) {
    return parsePad(text.substr(PAD_PREFIX.size()));
  }
  return std::nullopt;
}

std::string writeInputBindings(const InputBindings& bindings,
                               std::span<const KeyName> keys) {
  nlohmann::ordered_json actions = nlohmann::ordered_json::object();
  for (std::size_t i = 0; i < INPUT_ACTION_COUNT; ++i) {
    nlohmann::ordered_json list = nlohmann::ordered_json::array();
    for (const InputSource& source :
         bindings.sources(static_cast<InputAction>(i))) {
      list.push_back(inputSourceText(source, keys));
    }
    actions[std::string{ACTION_NAMES[i]}] = std::move(list);
  }
  const nlohmann::ordered_json root = {
      {"version", FORMAT_VERSION},
      {"deadzones", deadzonesJson(bindings.deadzones())},
      {"actions", std::move(actions)}};
  return root.dump(2) + "\n";
}

InputBindingsLoad parseInputBindings(std::string_view json,
                                     const InputBindings& defaults,
                                     std::span<const KeyName> keys) {
  InputBindingsLoad out{defaults, {}};
  const nlohmann::json root =
      nlohmann::json::parse(json.begin(), json.end(), nullptr, false);
  if (!root.is_object()) {
    out.problems.emplace_back("not a JSON object; using the default bindings");
    return out;
  }
  Reader reader{out, keys};
  readRoot(reader, root);
  return out;
}

}  // namespace eng::input
