#pragma once

/// @file ground-quarter-shape.h
/// @brief The shape one quarter of a cell takes, from its neighbours.
/// @par Threading Thread-safe (pure function over value types).

#include <cstdint>
#include <engine/render-ground/ground-cell.h>
#include <engine/render-ground/ground-grid.h>
#include <engine/render-ground/ground-quarter.h>

namespace eng {

/// What one layer draws in one quarter of one cell.
///
/// Four shapes are the whole of the autotiling: every join between painted
/// and unpainted cells is made of them, and each is decided by the quarter's
/// own cell and the three that meet it at its corner. Along any edge between
/// two quarters, both sides cover from the painted cell's centre to the
/// edge's midpoint and no further, so the shapes meet without a seam
/// whichever of them sit side by side.
/// @thread_safety Immutable value type.
enum class GroundQuarterShape : uint8_t {
  /// Nothing: the layer does not reach this quarter.
  EMPTY,
  /// The whole quarter: the cell is painted and continues past this corner.
  FULL,
  /// A quarter disc about the cell's centre, radius half a tile: the cell is
  /// painted and nothing continues past this corner, so the corner is
  /// rounded off. A cell painted on its own is a disc.
  ROUND,
  /// The quarter less that disc: the cell is not painted, but both cells
  /// beside this corner are, so the inside of the bend they make is filled
  /// with a curve rather than left a notch.
  FILLET,
};

/// The shape layer @p layer draws in @p quarter of @p cell.
///
/// A cell counts as painted for a layer when its terrain is that layer or a
/// later one, which is what makes the layers stack without gaps: a road
/// laid through sand has sand filled in under it, so the road's rounded
/// edges sit on sand rather than on bare ground. Layer 0 is bare ground and
/// draws nothing.
///
/// A painted cell rounds a corner only when no cell past it — beside it or
/// across it — is painted, so two cells touching only at a corner join
/// there: a road painted as a diagonal staircase reads as one road.
[[nodiscard]] GroundQuarterShape groundQuarterShape(const GroundGrid& grid,
                                                    uint8_t layer,
                                                    GroundCell cell,
                                                    GroundQuarter quarter);

}  // namespace eng
