#pragma once

/// @file editor-asset-thumbnail.h
/// @brief Rendering one asset's mesh to the picture its card shows.
/// @par Threading Thread-safe (pure function over the mesh it is given).

#include <cstdint>
#include <editor/shell/mesh-rasterizer.h>
#include <engine/gui/image-data.h>
#include <engine/render-mesh/mesh-data.h>

namespace eng::editor {

/// Side length of a generated thumbnail, in pixels.
///
/// Larger than the card draws it, so the picture still reads when the card
/// grows or the display is dense.
inline constexpr uint32_t ASSET_THUMBNAIL_SIZE = 128;

/// Fraction of the frame the model is fitted to, leaving a margin so
/// nothing touches the edge.
inline constexpr float ASSET_THUMBNAIL_FILL = 0.82f;

/// Render @p mesh into a square thumbnail @p size pixels on a side.
///
/// The camera is the editor's own: the same dimetric projection, the same
/// shading, so a card shows the model at the angle the viewport will show
/// it at once it is placed. What differs is the framing — the model is
/// centred and fitted to the frame rather than standing on a world tile,
/// since a thumbnail is for recognising an asset, not for placing it.
///
/// A mesh with no triangles, or a zero size, yields an empty image rather
/// than a blank one: there is a difference between "nothing to draw" and
/// "drew nothing", and the caller wants to tell them apart.
[[nodiscard]] ImageData renderAssetThumbnail(const MeshData& mesh,
                                             uint32_t size);

}  // namespace eng::editor
