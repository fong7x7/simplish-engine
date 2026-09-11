#pragma once

/// @file behavior-names.h
/// @brief The words behaviors are written in: actions, conditions, facings
/// and factions by name.
/// @par Threading
/// Pure functions.

#include <game/content/behavior-action.h>
#include <game/content/behavior-condition.h>
#include <game/content/behavior-facing.h>
#include <game/content/behavior-route-mode.h>
#include <game/content/faction.h>
#include <optional>
#include <string_view>

namespace eng::game {

/// @p action as a behaviors file writes it: `keep_distance`.
[[nodiscard]] std::string_view behaviorActionName(BehaviorAction action);
/// The action a behaviors file means by @p name, if any.
[[nodiscard]] std::optional<BehaviorAction>
parseBehaviorAction(std::string_view name);

/// @p condition as a behaviors file writes it: `lost_target_for`.
[[nodiscard]] std::string_view
behaviorConditionName(BehaviorCondition condition);
/// The condition a behaviors file means by @p name, if any.
[[nodiscard]] std::optional<BehaviorCondition>
parseBehaviorCondition(std::string_view name);

/// @p facing as a behaviors file writes it: `target`.
[[nodiscard]] std::string_view behaviorFacingName(BehaviorFacing facing);
/// The facing a behaviors file means by @p name, if any.
[[nodiscard]] std::optional<BehaviorFacing>
parseBehaviorFacing(std::string_view name);

/// @p mode as a behaviors file writes it: `ping_pong`.
[[nodiscard]] std::string_view behaviorRouteModeName(BehaviorRouteMode mode);
/// The route mode a behaviors file means by @p name, if any.
[[nodiscard]] std::optional<BehaviorRouteMode>
parseBehaviorRouteMode(std::string_view name);

/// @p faction as a level file writes it: `hostile`.
[[nodiscard]] std::string_view factionName(Faction faction);
/// The faction a level file means by @p name, if any.
[[nodiscard]] std::optional<Faction> parseFaction(std::string_view name);

}  // namespace eng::game
