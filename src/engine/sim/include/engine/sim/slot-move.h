#pragma once

/// @file slot-move.h
/// @brief One element moved by compaction, and how a pool field follows it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <span>

namespace eng::sim {

/// Compaction moved the entity at dense index `from` down to `to`, filling a
/// destroyed entity's place. A pool applies every move, in order, to every
/// one of its field arrays.
struct SlotMove {
  /// Dense index the entity was at — always the last live one at the time.
  uint32_t from = 0;
  /// Dense index it is at now.
  uint32_t to = 0;
};

/// Applies compaction's `moves` to one field array of a pool: a
/// `std::vector`, `std::array` or span indexed by dense index. Call it once
/// per field, with the span `EntitySlots::compact` returned:
///
/// @code
///   const auto moves = slots.compact();
///   applySlotMoves(moves, position);
///   applySlotMoves(moves, velocity);
/// @endcode
template <typename Field>
void applySlotMoves(std::span<const SlotMove> moves, Field& field) {
  for (const SlotMove& move : moves) {
    field[move.to] = field[move.from];
  }
}

}  // namespace eng::sim
