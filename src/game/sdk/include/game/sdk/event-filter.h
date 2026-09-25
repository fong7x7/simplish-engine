#pragma once

/// @file event-filter.h
/// @brief Which of the last tick's events a query is about.
/// @par Threading
/// A value type; its views must outlive the query it is used in.

#include <game/logic/logic-damage-cause.h>
#include <game/logic/logic-event-kind.h>
#include <game/logic/logic-event.h>
#include <game/logic/logic-target.h>
#include <optional>
#include <string_view>

namespace eng::game::sdk {

/// A test every event a query returns passes. Each field left empty lets
/// every event through; the default takes them all.
///
/// @code
///   // Everything player 1 killed last tick, and with what.
///   sdk::findEvents(world, {.kind = LogicEventKind::ACTOR_DIED,
///                           .by = player.target});
///   // Whether the boss was hurt by a blast.
///   sdk::heard(world, {.kind = LogicEventKind::ACTOR_HURT, .id = "boss",
///                      .cause = LogicDamageCause::BLAST});
/// @endcode
struct EventFilter {
  /// Only events of this kind.
  std::optional<LogicEventKind> kind{};
  /// Only events about this player or actor.
  std::optional<LogicTarget> target{};
  /// Only events about the actor named exactly this.
  std::string_view id{};
  /// Only events about actors whose name starts with this.
  std::string_view id_prefix{};
  /// Only events credited to this player or actor.
  std::optional<LogicTarget> by{};
  /// Only hurts, deaths and downings of this cause.
  std::optional<LogicDamageCause> cause{};
  /// Only state changes into the state with this id.
  std::string_view state{};
};

/// Whether @p event passes @p filter.
[[nodiscard]] bool matches(const EventFilter& filter, const LogicEvent& event);

}  // namespace eng::game::sdk
