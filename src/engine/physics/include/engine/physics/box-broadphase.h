#pragma once

/// @file box-broadphase.h
/// @brief Which of a level's boxes are near a point, without testing all
/// of them.
/// @par Threading
/// Immutable after construction; safe to read from any thread.

#include <array>
#include <cstdint>
#include <engine/math/vec2.h>
#include <engine/physics/collision-box.h>
#include <span>
#include <vector>

namespace eng::physics {

/// The side of one broadphase cell, in tiles.
inline constexpr float BOX_BROADPHASE_CELL_TILES = 2.0F;

/// The most cells along either side of a broadphase: a level 1,024 tiles
/// across. Beyond it cells get larger rather than more numerous.
inline constexpr uint32_t BOX_BROADPHASE_MAX_SIDE = 512;

/// The level's static boxes, bucketed by the floor they stand on — the
/// static-geometry broadphase in front of `resolveCylinderAgainstBoxes`
/// (Engine REQUIREMENTS §6). Built once, when the run starts, since static
/// geometry never moves; a box is listed in every cell its footprint
/// touches.
///
/// `gather` answers with box indices, ascending and each once, so resolving
/// against the candidates visits the boxes in the order the full list
/// holds them, as the full scan does.
class BoxBroadphase {
public:
  /// A broadphase with no boxes in it.
  BoxBroadphase() = default;

  /// Bucket @p boxes, which the broadphase does not keep.
  explicit BoxBroadphase(std::span<const physics::CollisionBox> boxes);

  /// Replace @p out with the index of every box whose footprint comes
  /// within @p reach of @p center, ascending, each once. A superset: the
  /// caller tests each. @p out is expected to be reserved for
  /// `candidateCapacity()`, so gathering does not allocate.
  void gather(Vec2 center, float reach, std::vector<uint32_t>& out) const;

  /// How many entries `gather` can write before it throws duplicates away:
  /// what a caller reserves.
  [[nodiscard]] uint32_t candidateCapacity() const { return capacity_; }

private:
  /// The cell column or row @p offset tiles from the origin lies in, held
  /// to the @p count there are.
  [[nodiscard]] uint32_t cellAlong(float offset, uint32_t count) const;
  /// The first and last cell column and row @p box's footprint touches:
  /// x0, y0, x1, y1.
  [[nodiscard]] std::array<uint32_t, 4>
  cellsUnder(const physics::CollisionBox& box) const;
  /// Count, then list, the boxes each cell's footprint range touches.
  void bucket(std::span<const physics::CollisionBox> boxes);

  /// World X and Y of the corner of cell (0, 0).
  Vec2 origin_{};
  /// The side of a cell, in tiles.
  float cell_ = BOX_BROADPHASE_CELL_TILES;
  /// Cell columns.
  uint32_t width_ = 0;
  /// Cell rows.
  uint32_t height_ = 0;
  /// Where each cell's boxes start in `entries_`, one more than there are
  /// cells.
  std::vector<uint32_t> start_;
  /// Box indices, cell by cell, ascending within each.
  std::vector<uint32_t> entries_;
  /// Enough room for every candidate any gather can list before
  /// deduplicating.
  uint32_t capacity_ = 0;
};

}  // namespace eng::physics
