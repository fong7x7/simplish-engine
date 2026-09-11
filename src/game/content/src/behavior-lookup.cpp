#include <algorithm>
#include <array>
#include <cstddef>
#include <game/content/behavior-lookup.h>

namespace eng::game {

namespace {

  /// The distances an action starts with before a file says otherwise.
  struct ActionDistances {
    /// Its `BehaviorState::near_tiles`.
    float near_tiles = 0.0F;
    /// Its `BehaviorState::far_tiles`.
    float far_tiles = 0.0F;
  };

  /// Each action's default distances, in enumerator order. A pursuit stops
  /// a little beyond touching (two 0.3-tile radii); a follower hangs back
  /// two tiles; keeping distance holds a band of three to six; a wander
  /// strays four tiles and a flight runs eight.
  constexpr std::array<ActionDistances, 15> ACTION_DISTANCES{{
      {},            // idle
      {},            // hold
      {0.0F, 4.0F},  // wander
      {0.9F, 0.0F},  // pursue
      {3.0F, 6.0F},  // keep_distance
      {0.0F, 8.0F},  // flee
      {2.0F, 0.0F},  // follow
      {},            // search
      {},            // return_home
      {},            // charge
      {},            // patrol
      {},            // melee: closes to touching, whatever it is
      {},            // fire
      {},            // spit
      {},            // detonate
  }};
  static_assert(ACTION_DISTANCES.size() ==
                static_cast<size_t>(BehaviorAction::DETONATE) + 1);

  /// Each action's default attack, in enumerator order: nothing for the
  /// ones that do not attack. A bite or a rush takes a segment and comes
  /// again after three quarters of a second or a second; a volley is three
  /// shots fanned over 24° at nine tiles a second, every second and a
  /// half; a pool is a tile in radius and lasts five seconds, lobbed every
  /// two and a half; a blast reaches two tiles and takes two.
  constexpr std::array<BehaviorAttack, 15> ACTION_ATTACKS{{
      {},
      {},
      {},
      {},
      {},
      {},
      {},
      {},
      {},                                                        // idle–return
      {.damage = 1, .cooldown_ticks = 60, .reach_tiles = 0.2F},  // charge
      {},                                                        // patrol
      {.damage = 1, .cooldown_ticks = 45, .reach_tiles = 0.3F},  // melee
      {.damage = 1,
       .cooldown_ticks = 90,
       .count = 3,
       .spread_degrees = 24.0F,
       .speed = 9.0F},  // fire
      {.damage = 1,
       .cooldown_ticks = 150,
       .radius = 1.0F,
       .duration_ticks = 300},        // spit
      {.damage = 2, .radius = 2.0F},  // detonate
  }};
  static_assert(ACTION_ATTACKS.size() == ACTION_DISTANCES.size());

  /// Whether every exit of @p exits leads to one of @p state_count states.
  bool exitsInRange(const std::vector<BehaviorExit>& exits,
                    size_t state_count) {
    return std::ranges::all_of(
        exits, [&](const BehaviorExit& e) { return e.to < state_count; });
  }

  /// The built-in behavior everything unresolvable runs: the first preset.
  const BehaviorDefinition& idleBehavior() {
    return builtInBehaviors().front();
  }

  /// The behavior in @p behaviors with @p id, if any.
  const BehaviorDefinition*
  findBehavior(std::span<const BehaviorDefinition> behaviors,
               std::string_view id) {
    for (const BehaviorDefinition& behavior : behaviors) {
      if (behavior.id == id) {
        return &behavior;
      }
    }
    return nullptr;
  }

}  // namespace

const BehaviorDefinition& resolveBehavior(const GameContent& content,
                                          std::string_view id) {
  const BehaviorDefinition* found = findBehavior(content.behaviors, id);
  if (found == nullptr) {
    found = findBehavior(builtInBehaviors(), id);
  }
  if (found == nullptr || !behaviorIsWellFormed(*found)) {
    return idleBehavior();
  }
  return *found;
}

bool behaviorIsWellFormed(const BehaviorDefinition& behavior) {
  const size_t count = behavior.states.size();
  if (count == 0 || count > BEHAVIOR_MAX_STATES || behavior.initial >= count ||
      !exitsInRange(behavior.interrupts, count)) {
    return false;
  }
  return std::ranges::all_of(behavior.states, [&](const BehaviorState& state) {
    return exitsInRange(state.exits, count);
  });
}

BehaviorState defaultBehaviorState(BehaviorAction action) {
  const ActionDistances distances =
      ACTION_DISTANCES[static_cast<size_t>(action)];
  BehaviorState state;
  state.action = action;
  state.near_tiles = distances.near_tiles;
  state.far_tiles = distances.far_tiles;
  state.attack = ACTION_ATTACKS[static_cast<size_t>(action)];
  return state;
}

}  // namespace eng::game
