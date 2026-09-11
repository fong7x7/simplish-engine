#include <array>
#include <game/content/behavior-lookup.h>
#include <string>
#include <utility>
#include <vector>

// The built-in behaviors, written as data. Each preset names its states by
// index through a local enum, so an exit reads `to = PURSUE` rather than a
// bare number, and the order of the enum is the order of the states.

namespace eng::game {

namespace {

  using Cond = BehaviorCondition;
  using Act = BehaviorAction;

  /// An exit taken when @p when holds, needing no number.
  BehaviorExit on(Cond when, uint8_t to) {
    return {.when = when, .to = to};
  }

  /// An exit taken when @p when holds of @p ticks.
  BehaviorExit onTicks(Cond when, uint32_t ticks, uint8_t to) {
    return {.when = when, .ticks = ticks, .to = to};
  }

  /// An exit taken when @p when holds of @p tiles.
  BehaviorExit onTiles(Cond when, float tiles, uint8_t to) {
    return {.when = when, .tiles = tiles, .to = to};
  }

  /// A state called @p id doing @p action, with its default distances.
  BehaviorState state(std::string id, Act action,
                      std::vector<BehaviorExit> exits) {
    BehaviorState made = defaultBehaviorState(action);
    made.id = std::move(id);
    made.exits = std::move(exits);
    return made;
  }

  /// @p made facing @p facing.
  BehaviorState facing(BehaviorState made, BehaviorFacing toward) {
    made.facing = toward;
    return made;
  }

  /// A behavior called @p name, with the default senses and movement.
  BehaviorDefinition behavior(std::string id, std::string name,
                              std::vector<BehaviorState> states) {
    BehaviorDefinition made;
    made.id = std::move(id);
    made.name = std::move(name);
    made.states = std::move(states);
    return made;
  }

  /// Stands where it is placed, turning to watch whoever it sees.
  BehaviorDefinition idle() {
    return behavior("idle", "Idle", {state("idle", Act::IDLE, {})});
  }

  /// Ambles around the spot it is placed on, paying nobody any mind.
  BehaviorDefinition wander() {
    BehaviorDefinition made =
        behavior("wander", "Wander", {state("wander", Act::WANDER, {})});
    made.movement.speed = 1.5F;
    return made;
  }

  /// Watches its post, gives chase within a leash, searches where it lost
  /// sight of its quarry, then goes back.
  BehaviorDefinition guard() {
    enum : uint8_t { WATCH, PURSUE, SEARCH, RETURN };
    return behavior(
        "guard", "Guard",
        {state("watch", Act::IDLE,
               {on(Cond::SEES_TARGET, PURSUE), on(Cond::HEARS_TARGET, SEARCH)}),
         state("pursue", Act::PURSUE,
               {onTiles(Cond::FAR_FROM_HOME, 12.0F, RETURN),
                onTicks(Cond::LOST_TARGET_FOR, 120, SEARCH)}),
         state("search", Act::SEARCH,
               {on(Cond::SEES_TARGET, PURSUE),
                onTicks(Cond::IN_STATE_FOR, 300, RETURN)}),
         state("return_home", Act::RETURN_HOME, {on(Cond::ARRIVED, WATCH)})});
  }

  /// Runs at whoever it sees or hears, all round, and keeps coming: the
  /// swarmer of Game §5.1.
  BehaviorDefinition chase() {
    enum : uint8_t { IDLE, PURSUE, SEARCH };
    BehaviorDefinition made = behavior(
        "chase", "Chase",
        {state("idle", Act::IDLE,
               {on(Cond::SEES_TARGET, PURSUE), on(Cond::HEARS_TARGET, PURSUE)}),
         state("pursue", Act::PURSUE,
               {onTicks(Cond::LOST_TARGET_FOR, 300, SEARCH)}),
         state("search", Act::SEARCH,
               {on(Cond::SEES_TARGET, PURSUE), on(Cond::HEARS_TARGET, PURSUE),
                onTicks(Cond::IN_STATE_FOR, 300, IDLE)})});
    made.senses.sight_range = 14.0F;
    made.senses.view_degrees = 360.0F;
    made.movement.speed = 4.0F;
    return made;
  }

  /// Keeps its quarry at arm's length and watches them while it does: the
  /// ranged enemy of Game §5.1, less the shooting.
  BehaviorDefinition skirmisher() {
    enum : uint8_t { IDLE, KEEP, SEARCH };
    return behavior(
        "skirmisher", "Skirmisher",
        {state("idle", Act::IDLE, {on(Cond::SEES_TARGET, KEEP)}),
         facing(state("keep_distance", Act::KEEP_DISTANCE,
                      {onTicks(Cond::LOST_TARGET_FOR, 240, SEARCH)}),
                BehaviorFacing::TARGET),
         state("search", Act::SEARCH,
               {on(Cond::SEES_TARGET, KEEP),
                onTicks(Cond::IN_STATE_FOR, 240, IDLE)})});
  }

  /// Wanders until it sees someone, then runs until it has not for a while.
  BehaviorDefinition coward() {
    enum : uint8_t { WANDER, FLEE };
    BehaviorDefinition made =
        behavior("coward", "Coward",
                 {state("wander", Act::WANDER, {on(Cond::SEES_TARGET, FLEE)}),
                  state("flee", Act::FLEE,
                        {onTicks(Cond::LOST_TARGET_FOR, 180, WANDER)})});
    made.movement.speed = 4.5F;
    return made;
  }

  /// Keeps up with whoever it sees, and waits where it lost them.
  BehaviorDefinition follower() {
    enum : uint8_t { IDLE, FOLLOW, SEARCH };
    return behavior(
        "follower", "Follower",
        {state("idle", Act::IDLE, {on(Cond::SEES_TARGET, FOLLOW)}),
         state("follow", Act::FOLLOW,
               {onTicks(Cond::LOST_TARGET_FOR, 300, SEARCH)}),
         state("search", Act::SEARCH,
               {on(Cond::SEES_TARGET, FOLLOW), on(Cond::ARRIVED, IDLE)})});
  }

  /// The charger's rush and its recovery, the two states that make it a
  /// charger: straight ahead at nearly three times its walk, not turning.
  std::vector<BehaviorState> chargerRush(uint8_t recover, uint8_t pursue) {
    BehaviorState rush = facing(state("rush", Act::CHARGE,
                                      {onTicks(Cond::IN_STATE_FOR, 24, recover),
                                       on(Cond::BLOCKED, recover)}),
                                BehaviorFacing::LOCKED);
    rush.speed_permille = 2800;
    rush.clip = "run";
    return {rush, facing(state("recover", Act::HOLD,
                               {onTicks(Cond::IN_STATE_FOR, 40, pursue)}),
                         BehaviorFacing::LOCKED)};
  }

  /// The charger's states from spotting its quarry to winding up to rush.
  std::vector<BehaviorState> chargerApproach(uint8_t pursue, uint8_t windup,
                                             uint8_t rush, uint8_t search) {
    BehaviorState wind = facing(
        state("windup", Act::HOLD, {onTicks(Cond::IN_STATE_FOR, 30, rush)}),
        BehaviorFacing::TARGET);
    wind.clip = "windup";
    return {state("idle", Act::IDLE, {on(Cond::SEES_TARGET, pursue)}),
            state("pursue", Act::PURSUE,
                  {onTiles(Cond::TARGET_WITHIN, 4.0F, windup),
                   onTicks(Cond::LOST_TARGET_FOR, 180, search)}),
            wind};
  }

  /// The charger's states once it has lost its quarry: look, then go home.
  std::vector<BehaviorState> chargerGiveUp(uint8_t rest, uint8_t pursue,
                                           uint8_t go_home) {
    return {state("search", Act::SEARCH,
                  {on(Cond::SEES_TARGET, pursue),
                   onTicks(Cond::IN_STATE_FOR, 120, go_home)}),
            state("return_home", Act::RETURN_HOME, {on(Cond::ARRIVED, rest)})};
  }

  /// @p more moved onto the end of @p states.
  void append(std::vector<BehaviorState>& states,
              std::vector<BehaviorState> more) {
    for (BehaviorState& next : more) {
      states.push_back(std::move(next));
    }
  }

  /// Closes in, winds up facing its quarry, then rushes in a straight line
  /// that overshoots, and recovers: the charger of Game §5.1.
  BehaviorDefinition charger() {
    enum : uint8_t { IDLE, PURSUE, WINDUP, RUSH, RECOVER, SEARCH, RETURN };
    std::vector<BehaviorState> states =
        chargerApproach(PURSUE, WINDUP, RUSH, SEARCH);
    append(states, chargerRush(RECOVER, PURSUE));
    append(states, chargerGiveUp(IDLE, PURSUE, RETURN));
    return behavior("charger", "Charger", std::move(states));
  }

}  // namespace

std::span<const BehaviorDefinition> builtInBehaviors() {
  static const std::array<BehaviorDefinition, 8> presets{
      idle(),       wander(), guard(),    chase(),
      skirmisher(), coward(), follower(), charger()};
  return presets;
}

}  // namespace eng::game
