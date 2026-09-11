#include "editor-behavior-row.h"

#include <algorithm>
#include <cstdint>
#include <editor/shell/editor-entity-id.h>
#include <game/content/behavior-lookup.h>
#include <game/content/behavior-names.h>
#include <limits>

// Reading one behavior row. Every reader here takes a `RowRead`, which names
// the row and collects its problems, so each can say what it skipped without
// threading a row id and a problem list through every call.

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// Furthest a sense reaches, in tiles: well past a screen.
  constexpr float MAX_RANGE_TILES = 100.0F;
  /// Longest a duration may be, in ticks: ten minutes.
  constexpr float MAX_TICKS = 36000.0F;
  /// Fastest a behavior may move, in tiles a second: a character's limit.
  constexpr float MAX_SPEED = 20.0F;
  /// Fastest a behavior may turn, in degrees a second: ten turns.
  constexpr float MAX_TURN = 3600.0F;
  /// Fastest a state may move, in thousandths of its behavior's speed.
  constexpr float MAX_SPEED_PERMILLE = 5000.0F;

  /// The row being read, and where its problems go.
  struct RowRead {
    /// The row's id, which starts every problem line.
    const std::string& id;
    /// Where problems go.
    std::vector<std::string>& problems;
  };

  /// A number's default and the range it is held to.
  struct NumberRule {
    /// What an absent or unreadable number is.
    float fallback = 0.0F;
    /// The least it may be.
    float low = 0.0F;
    /// The most it may be.
    float high = 0.0F;
  };

  /// Say @p what is wrong with the row.
  void note(const RowRead& row, const std::string& what) {
    row.problems.push_back(row.id + ": " + what);
  }

  /// The string under @p key of @p object, or empty.
  std::string stringAt(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string()
               ? found->get<std::string>()
               : std::string{};
  }

  /// The number under @p key of @p object, by @p rule, saying so when it is
  /// not a number or was held to its range.
  float numberAt(const json& object, const char* key, NumberRule rule,
                 const RowRead& row) {
    const auto found = object.find(key);
    if (found == object.end()) {
      return rule.fallback;
    }
    if (!found->is_number()) {
      note(row, std::string(key) + " is not a number, so it is the default");
      return rule.fallback;
    }
    const auto value = found->get<float>();
    const float held = std::clamp(value, rule.low, rule.high);
    if (held != value) {
      note(row, std::string(key) + " was held to " + std::to_string(held));
    }
    return held;
  }

  /// The object under @p key of @p object, or an empty one.
  json objectAt(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_object() ? *found
                                                       : json::object();
  }

  /// The array under @p key of @p object, or an empty one.
  json arrayAt(const json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_array() ? *found : json::array();
  }

  /// A row's senses, from its `senses` object.
  game::BehaviorSenses readSenses(const json& entry, const RowRead& row) {
    const json senses = objectAt(entry, "senses");
    const game::BehaviorSenses base{};
    return {
        numberAt(senses, "sight_range",
                 {base.sight_range, 0.0F, MAX_RANGE_TILES}, row),
        numberAt(senses, "view_degrees", {base.view_degrees, 0.0F, 360.0F},
                 row),
        numberAt(senses, "hearing_range",
                 {base.hearing_range, 0.0F, MAX_RANGE_TILES}, row),
        static_cast<uint32_t>(numberAt(
            senses, "memory_ticks",
            {static_cast<float>(base.memory_ticks), 0.0F, MAX_TICKS}, row))};
  }

  /// A row's movement, from its `movement` object.
  game::BehaviorMovement readMovement(const json& entry, const RowRead& row) {
    const json movement = objectAt(entry, "movement");
    const game::BehaviorMovement base{};
    return {numberAt(movement, "speed", {base.speed, 0.0F, MAX_SPEED}, row),
            numberAt(movement, "turn_degrees_per_second",
                     {base.turn_degrees_per_second, 0.0F, MAX_TURN}, row)};
  }

  /// The action @p state's `do` names, or `idle`, saying so, for a word
  /// that names none.
  game::BehaviorAction readAction(const json& state, const RowRead& row) {
    const std::string word = stringAt(state, "do");
    const auto action = game::parseBehaviorAction(word);
    if (!action && !word.empty()) {
      note(row, "\"" + word + "\" is not an action, so the state idles");
    }
    return action.value_or(game::BehaviorAction::IDLE);
  }

  /// The facing @p state's `face` names, or `movement`, saying so, for a
  /// word that names none.
  game::BehaviorFacing readFacing(const json& state, const RowRead& row) {
    const std::string word = stringAt(state, "face");
    const auto facing = game::parseBehaviorFacing(word);
    if (!facing && !word.empty()) {
      note(row,
           "\"" + word + "\" is not a facing, so the state faces movement");
    }
    return facing.value_or(game::BehaviorFacing::MOVEMENT);
  }

  /// The route mode @p state's `route` names, or `loop`, saying so, for a
  /// word that names none.
  game::BehaviorRouteMode readRouteMode(const json& state, const RowRead& row) {
    const std::string word = stringAt(state, "route");
    const auto mode = game::parseBehaviorRouteMode(word);
    if (!mode && !word.empty()) {
      note(row, "\"" + word + "\" is not a route mode, so the patrol loops");
    }
    return mode.value_or(game::BehaviorRouteMode::LOOP);
  }

  /// A distance of @p state under @p key, its default @p fallback.
  float distanceAt(const json& state, const char* key, float fallback,
                   const RowRead& row) {
    return numberAt(state, key, {fallback, 0.0F, MAX_RANGE_TILES}, row);
  }

  /// @p made's distances, read under the keys its action names them by.
  void readDistances(const json& state, game::BehaviorState& made,
                     const RowRead& row) {
    using game::BehaviorAction;
    const BehaviorAction action = made.action;
    if (action == BehaviorAction::PURSUE || action == BehaviorAction::FOLLOW) {
      made.near_tiles = distanceAt(state, "stop_within", made.near_tiles, row);
    } else if (action == BehaviorAction::KEEP_DISTANCE) {
      made.near_tiles = distanceAt(state, "min", made.near_tiles, row);
      made.far_tiles = std::max(distanceAt(state, "max", made.far_tiles, row),
                                made.near_tiles);
    } else if (action == BehaviorAction::WANDER) {
      made.far_tiles = distanceAt(state, "radius", made.far_tiles, row);
    } else if (action == BehaviorAction::FLEE) {
      made.far_tiles = distanceAt(state, "distance", made.far_tiles, row);
    }
  }

  /// One state, its exits not yet read.
  game::BehaviorState readState(const json& state, const std::string& id,
                                const RowRead& row) {
    game::BehaviorState made =
        game::defaultBehaviorState(readAction(state, row));
    made.id = id;
    made.facing = readFacing(state, row);
    made.speed_permille = static_cast<uint16_t>(numberAt(
        state, "speed_permille",
        {static_cast<float>(made.speed_permille), 0.0F, MAX_SPEED_PERMILLE},
        row));
    made.clip = stringAt(state, "clip");
    made.route = readRouteMode(state, row);
    readDistances(state, made, row);
    return made;
  }

  /// The index of the state of @p states called @p id, if any.
  std::optional<uint8_t>
  stateIndex(const std::vector<game::BehaviorState>& states,
             const std::string& id) {
    for (size_t i = 0; i < states.size(); ++i) {
      if (states[i].id == id) {
        return static_cast<uint8_t>(i);
      }
    }
    return std::nullopt;
  }

  /// Whether @p id can name another state of @p states; says why not.
  bool usableStateId(const std::vector<game::BehaviorState>& states,
                     const std::string& id, const RowRead& row) {
    if (id.empty() || makeEditorIdentifier(id) != id) {
      note(row, "a state whose id is missing or not an id was skipped");
      return false;
    }
    if (stateIndex(states, id)) {
      note(row, id + ": a second state with this id was skipped");
      return false;
    }
    if (states.size() >= game::BEHAVIOR_MAX_STATES) {
      note(row, id + ": past the most states a behavior may have, so skipped");
      return false;
    }
    return true;
  }

  /// One exit, or nothing, saying why, when it cannot be followed.
  std::optional<game::BehaviorExit>
  readExit(const json& exit, const std::vector<game::BehaviorState>& states,
           const RowRead& row) {
    const std::string when = exit.is_object() ? stringAt(exit, "when") : "";
    const auto condition = game::parseBehaviorCondition(when);
    const std::string to = exit.is_object() ? stringAt(exit, "to") : "";
    const auto target = stateIndex(states, to);
    if (!condition || !target) {
      note(row, "an exit (\"" + when + "\" to \"" + to +
                    "\") names no condition or no state, so was skipped");
      return std::nullopt;
    }
    return game::BehaviorExit{
        *condition, numberAt(exit, "tiles", {0.0F, 0.0F, MAX_RANGE_TILES}, row),
        static_cast<uint32_t>(
            numberAt(exit, "ticks", {0.0F, 0.0F, MAX_TICKS}, row)),
        static_cast<uint16_t>(
            numberAt(exit, "permille", {0.0F, 0.0F, 1000.0F}, row)),
        *target};
  }

  /// Every exit of @p exits that can be followed among @p states.
  std::vector<game::BehaviorExit>
  readExits(const json& exits, const std::vector<game::BehaviorState>& states,
            const RowRead& row) {
    std::vector<game::BehaviorExit> read;
    for (const json& exit : exits) {
      if (auto made = readExit(exit, states, row)) {
        read.push_back(*made);
      }
    }
    return read;
  }

  /// Every usable state of @p entry, in order, with the JSON its exits are
  /// read from once every state's id is known.
  std::vector<game::BehaviorState>
  readStates(const json& entry, std::vector<json>& exits, const RowRead& row) {
    std::vector<game::BehaviorState> states;
    for (const json& state : arrayAt(entry, "states")) {
      const std::string id = state.is_object() ? stringAt(state, "id") : "";
      if (usableStateId(states, id, row)) {
        states.push_back(readState(state, id, row));
        exits.push_back(arrayAt(state, "exits"));
      }
    }
    return states;
  }

  /// The state @p entry's `initial` names, or the first, saying so when it
  /// names one the row does not have.
  uint8_t readInitial(const json& entry,
                      const std::vector<game::BehaviorState>& states,
                      const RowRead& row) {
    const std::string initial = stringAt(entry, "initial");
    const auto found = stateIndex(states, initial);
    if (!found && !initial.empty()) {
      note(row, "initial \"" + initial + "\" is no state, so it is the first");
    }
    return found.value_or(0);
  }

  /// Every usable state of @p entry with its exits, and the row's
  /// interrupts, into @p made.
  void readMachine(const json& entry, game::BehaviorDefinition& made,
                   const RowRead& row) {
    std::vector<json> exits;
    made.states = readStates(entry, exits, row);
    for (size_t i = 0; i < made.states.size(); ++i) {
      made.states[i].exits = readExits(exits[i], made.states, row);
    }
    made.initial = readInitial(entry, made.states, row);
    made.interrupts = readExits(arrayAt(entry, "interrupts"), made.states, row);
  }

}  // namespace

std::optional<game::BehaviorDefinition>
readEditorBehaviorRow(const json& entry, const std::string& id,
                      std::vector<std::string>& problems) {
  const RowRead row{id, problems};
  game::BehaviorDefinition made;
  readMachine(entry, made, row);
  if (made.states.empty()) {
    note(row, "a behavior with no usable states was skipped");
    return std::nullopt;
  }
  made.id = id;
  made.name = stringAt(entry, "name").empty() ? id : stringAt(entry, "name");
  made.senses = readSenses(entry, row);
  made.movement = readMovement(entry, row);
  return made;
}

}  // namespace eng::editor
