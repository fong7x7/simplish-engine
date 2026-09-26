#pragma once

/// @file schedule.h
/// @brief Things the logic means to do on a later tick.
/// @par Threading
/// A value type; a member of the logic's.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <game/logic/game-logic-hash.h>
#include <game/logic/game-logic-world.h>
#include <optional>
#include <utility>
#include <vector>

namespace eng::game::sdk {

/// Values the logic has set aside for a play tick to come — `playTick`,
/// which a pause stops — "blow the bridge in
/// three seconds", "the second phase starts at minute two" — held until
/// that tick, rather than a timer checked every tick for each.
///
/// Ordered by tick, and by the order they were added within one, so what
/// comes due comes out the same on every machine; `hashInto` folds it all
/// in. `runDue` hands each value whose tick has come to a function, and
/// one it adds while running — even for now — comes out in the same call.
///
/// @code
///   struct Blow { LogicTarget bridge; };
///   sdk::Schedule<Blow> later_;
///
///   later_.after(world, sdk::seconds(3), {bridge});     // somewhere
///   later_.runDue(world, [](GameLogicWorld& world, const Blow& b) {
///     world.damage(b.bridge, 99);                         // in onTick
///   });
/// @endcode
template <typename Value> class Schedule {
public:
  /// One entry: the tick it is due on, and the value.
  using Entry = std::pair<uint64_t, Value>;

  /// Hold @p value until @p tick.
  void at(uint64_t tick, const Value& value);

  /// Hold @p value until @p ticks of play from now — `playTick`, which a
  /// pause stops.
  void after(const GameLogicWorld& world, uint64_t ticks, const Value& value) {
    at(world.playTick() + ticks, value);
  }

  /// Hand every value due by @p world's `playTick` to @p run — as
  /// `run(world, value)` — in order, taking each out first.
  template <typename Run> void runDue(GameLogicWorld& world, Run run);

  /// Drop every value @p drop says to, whenever it was due.
  template <typename Drop> void cancel(Drop drop) {
    std::erase_if(entries_, [&drop](const Entry& e) { return drop(e.second); });
  }

  /// Values held.
  [[nodiscard]] size_t size() const { return entries_.size(); }

  /// The tick the next value is due on; nothing when none is held.
  [[nodiscard]] std::optional<uint64_t> nextTick() const {
    return entries_.empty() ? std::nullopt
                            : std::optional(entries_.front().first);
  }

  /// Fold every entry into @p hash, in order: the tick, then the value,
  /// which must be something `GameLogicHash::add` takes.
  void hashInto(GameLogicHash& hash) const;

private:
  /// Every value held, by tick, then by when it was added.
  std::vector<Entry> entries_{};
};

template <typename Value>
void Schedule<Value>::at(uint64_t tick, const Value& value) {
  // After every entry due on the same tick: those added first come first.
  const auto place = std::upper_bound(
      entries_.begin(), entries_.end(), tick,
      [](uint64_t due, const Entry& entry) { return due < entry.first; });
  entries_.insert(place, Entry{tick, value});
}

template <typename Value>
template <typename Run>
void Schedule<Value>::runDue(GameLogicWorld& world, Run run) {
  while (!entries_.empty() && entries_.front().first <= world.playTick()) {
    const Value value = std::move(entries_.front().second);
    entries_.erase(entries_.begin());
    run(world, value);
  }
}

template <typename Value>
void Schedule<Value>::hashInto(GameLogicHash& hash) const {
  hash.add(entries_.size());
  for (const Entry& entry : entries_) {
    hash.add(entry.first);
    hash.add(entry.second);
  }
}

}  // namespace eng::game::sdk
