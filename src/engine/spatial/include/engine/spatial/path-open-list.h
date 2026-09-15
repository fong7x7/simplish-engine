#pragma once

/// @file path-open-list.h
/// @brief The cells a path search has reached and not yet expanded, in the
/// order it expands them.
/// @par Threading
/// One instance per `PathFinder`.

#include <array>
#include <cstdint>
#include <vector>

namespace eng::spatial {

/// How many estimated totals the open list holds at once: more than the
/// widest spread a consistent heuristic allows between its cells.
inline constexpr uint32_t PATH_OPEN_SPAN = 32;

/// `PathFinder`'s open list: its cells ordered by estimated total, then by
/// estimated remainder, then by row-major index — a total order, so a
/// search expands its cells in the same order on every machine.
///
/// **Buckets by total.** A* with a consistent heuristic takes cells off in
/// order of total, and a cell it reaches has a total at most two steps'
/// cost — 28 — above its parent's, which was the smallest open. So every
/// open cell's total is within `PATH_OPEN_SPAN` of the smallest, and the
/// list is a ring of buckets, one per total, each a small binary heap of
/// its cells by remainder then index packed into one 64-bit key. Taking
/// the first cell steps to the first bucket with any and pops its heap:
/// a few levels of integer comparisons, where one heap of every open cell
/// was a dozen levels of two-part ones.
///
/// **Entries are never moved.** A cell whose cost improves is pushed again,
/// and the caller skips the older entry when it comes first: its total is
/// larger, so it always comes after the improved one.
///
/// Push only totals from the smallest open to `PATH_OPEN_SPAN` above it;
/// the ring holds nothing further.
class PathOpenList {
public:
  /// Empty the list, the smallest total to come being @p first_total.
  void clear(uint32_t first_total);
  /// Put the cell at @p index on the list, at estimated total @p total and
  /// remainder @p rest.
  void push(uint32_t total, uint32_t rest, uint32_t index);
  /// Whether nothing is on the list.
  [[nodiscard]] bool empty() const;
  /// Take the cell that comes first off the list: its index. The list must
  /// not be empty.
  uint32_t take();

private:
  /// The bucket of the smallest total with any cells, found by stepping
  /// on from the last.
  std::vector<uint64_t>& firstBucket();

  /// Each bucket's cells, a min-heap of their remainders in the high half
  /// and indices in the low; bucket `t % PATH_OPEN_SPAN` holds total `t`.
  std::array<std::vector<uint64_t>, PATH_OPEN_SPAN> buckets_{};
  /// The smallest total that may have cells: no bucket before it has any.
  uint32_t total_ = 0;
  /// Cells on the list, in every bucket.
  uint32_t size_ = 0;
};

}  // namespace eng::spatial
