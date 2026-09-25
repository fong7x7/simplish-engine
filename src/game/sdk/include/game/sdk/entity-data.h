#pragma once

/// @file entity-data.h
/// @brief The logic's own data, kept per player or actor.
/// @par Threading
/// A value type; a member of the logic's.

#include <algorithm>
#include <cstddef>
#include <game/logic/game-logic-hash.h>
#include <game/logic/game-logic-world.h>
#include <game/logic/logic-target.h>
#include <tuple>
#include <utility>
#include <vector>

namespace eng::game::sdk {

/// Whether @p a sorts before @p b: by pool, then slot, then generation.
[[nodiscard]] constexpr bool targetBefore(const LogicTarget& a,
                                          const LogicTarget& b) {
  return std::tuple(a.kind, a.index, a.generation) <
         std::tuple(b.kind, b.index, b.generation);
}

/// A value of the logic's own for each player or actor it gives one to —
/// a score, a mark, a timer, a custom component — kept by target. The
/// entities are the engine's; this is how the logic hangs its own state on
/// them.
///
/// Ordered by target, never by address or hash, so iterating it is the
/// same on every machine; and `hashInto` folds it into the tick hash, as
/// every member the logic decides by must be.
///
/// @code
///   struct Rage { uint32_t hits = 0; };
///   sdk::EntityData<Rage> rage_;
///   rage_[event.target].hits += 1;   // in onActorHurt
///   rage_.forgetGone(world);         // now and then
/// @endcode
template <typename Value> class EntityData {
public:
  /// One entry: whose, and what.
  using Entry = std::pair<LogicTarget, Value>;

  /// @p target's value, made with `Value{}` when it has none yet.
  Value& operator[](const LogicTarget& target) {
    const auto at = lowerBound(target);
    if (at != entries_.end() && at->first == target) {
      return at->second;
    }
    return entries_.insert(at, Entry{target, Value{}})->second;
  }

  /// @p target's value, or null when it has none.
  [[nodiscard]] Value* find(const LogicTarget& target) {
    const auto at = lowerBound(target);
    return at != entries_.end() && at->first == target ? &at->second : nullptr;
  }

  /// @p target's value, or null when it has none.
  [[nodiscard]] const Value* find(const LogicTarget& target) const {
    const auto at =
        std::lower_bound(entries_.begin(), entries_.end(), target, entryBefore);
    return at != entries_.end() && at->first == target ? &at->second : nullptr;
  }

  /// Forget @p target's value.
  void erase(const LogicTarget& target);

  /// Forget the values of every player and actor no longer in @p world.
  void forgetGone(const GameLogicWorld& world) {
    std::erase_if(entries_, [&world](const Entry& entry) {
      return !world.actorOf(entry.first) && !world.playerOf(entry.first);
    });
  }

  /// Entries held.
  [[nodiscard]] size_t size() const { return entries_.size(); }

  /// The entries, ordered by target.
  [[nodiscard]] auto begin() const { return entries_.begin(); }
  /// The end of the entries.
  [[nodiscard]] auto end() const { return entries_.end(); }

  /// Fold every entry into @p hash, in order: the target, then the value,
  /// which must be something `GameLogicHash::add` takes.
  void hashInto(GameLogicHash& hash) const;

private:
  /// Whether @p entry sorts before @p target.
  static bool entryBefore(const Entry& entry, const LogicTarget& target) {
    return targetBefore(entry.first, target);
  }

  /// The first entry not before @p target.
  auto lowerBound(const LogicTarget& target) {
    return std::lower_bound(entries_.begin(), entries_.end(), target,
                            entryBefore);
  }

  /// Every entry, ordered by target.
  std::vector<Entry> entries_{};
};

template <typename Value>
void EntityData<Value>::erase(const LogicTarget& target) {
  const auto at = lowerBound(target);
  if (at != entries_.end() && at->first == target) {
    entries_.erase(at);
  }
}

template <typename Value>
void EntityData<Value>::hashInto(GameLogicHash& hash) const {
  hash.add(entries_.size());
  for (const Entry& entry : entries_) {
    hash.add(entry.first.kind);
    hash.add(entry.first.index);
    hash.add(entry.first.generation);
    hash.add(entry.second);
  }
}

}  // namespace eng::game::sdk
