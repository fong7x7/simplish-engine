#include <array>
#include <cstddef>
#include <game/content/behavior-names.h>

namespace eng::game {

namespace {

  /// Actions by name, in enumerator order.
  constexpr std::array<std::string_view, 10> ACTION_NAMES{
      "idle", "hold",   "wander", "pursue",      "keep_distance",
      "flee", "follow", "search", "return_home", "charge"};
  static_assert(ACTION_NAMES.size() ==
                static_cast<size_t>(BehaviorAction::CHARGE) + 1);

  /// Conditions by name, in enumerator order.
  constexpr std::array<std::string_view, 12> CONDITION_NAMES{
      "always",        "sees_target",   "hears_target",  "lost_target_for",
      "target_within", "target_beyond", "in_state_for",  "arrived",
      "no_path",       "blocked",       "far_from_home", "chance"};
  static_assert(CONDITION_NAMES.size() ==
                static_cast<size_t>(BehaviorCondition::CHANCE) + 1);

  /// Facings by name, in enumerator order.
  constexpr std::array<std::string_view, 3> FACING_NAMES{"movement", "target",
                                                         "locked"};
  static_assert(FACING_NAMES.size() ==
                static_cast<size_t>(BehaviorFacing::LOCKED) + 1);

  /// Factions by name, in enumerator order.
  constexpr std::array<std::string_view, 3> FACTION_NAMES{"hostile", "neutral",
                                                          "friendly"};
  static_assert(FACTION_NAMES.size() == ALL_FACTIONS.size());

  /// The enumerator of @p names whose name is @p name, if any.
  template <typename Enum, size_t N>
  std::optional<Enum> parseName(const std::array<std::string_view, N>& names,
                                std::string_view name) {
    for (size_t i = 0; i < N; ++i) {
      if (names[i] == name) {
        return static_cast<Enum>(i);
      }
    }
    return std::nullopt;
  }

}  // namespace

std::string_view behaviorActionName(BehaviorAction action) {
  return ACTION_NAMES[static_cast<size_t>(action)];
}

std::optional<BehaviorAction> parseBehaviorAction(std::string_view name) {
  return parseName<BehaviorAction>(ACTION_NAMES, name);
}

std::string_view behaviorConditionName(BehaviorCondition condition) {
  return CONDITION_NAMES[static_cast<size_t>(condition)];
}

std::optional<BehaviorCondition> parseBehaviorCondition(std::string_view name) {
  return parseName<BehaviorCondition>(CONDITION_NAMES, name);
}

std::string_view behaviorFacingName(BehaviorFacing facing) {
  return FACING_NAMES[static_cast<size_t>(facing)];
}

std::optional<BehaviorFacing> parseBehaviorFacing(std::string_view name) {
  return parseName<BehaviorFacing>(FACING_NAMES, name);
}

std::string_view factionName(Faction faction) {
  return FACTION_NAMES[static_cast<size_t>(faction)];
}

std::optional<Faction> parseFaction(std::string_view name) {
  return parseName<Faction>(FACTION_NAMES, name);
}

}  // namespace eng::game
