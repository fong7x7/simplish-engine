#pragma once

/// @file logic-event.h
/// @brief One thing that happened in the last tick, as game logic hears it.
/// @par Threading
/// A value type; its id is a view valid until the logic's `tick` returns.

#include <engine/math/vec3.h>
#include <game/logic/logic-event-kind.h>
#include <game/logic/logic-target.h>
#include <string_view>

namespace eng::game {

/// What happened, to whom, and where. An actor that died or was removed is
/// gone by the time its event is read, so the event carries what the logic
/// would otherwise have asked it: where it was, and its name.
struct LogicEvent {
  /// What happened.
  LogicEventKind kind = LogicEventKind::ACTOR_DIED;
  /// To whom. For a death or a removal, no longer anyone.
  LogicTarget target{};
  /// Where they were when it happened, in tiles.
  Vec3 at{};
  /// The actor's name — the level's, or the logic's — or empty for a
  /// player or an actor given none.
  std::string_view id{};
};

}  // namespace eng::game
