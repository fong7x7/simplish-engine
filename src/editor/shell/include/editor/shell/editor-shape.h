#pragma once

/// @file editor-shape.h
/// @brief The geometry and the asset record behind a built-in shape.
/// @par Threading Main-thread-only (produces meshes and asset records).

#include <cstddef>
#include <editor/shell/editor-asset.h>
#include <editor/shell/editor-shape-kind.h>
#include <engine/render-mesh/mesh-data.h>
#include <vector>

namespace eng::editor {

/// The geometry a shape is, already in the world's axes and standing on the
/// ground plane in a unit box — see `mesh-primitives.h`.
///
/// Generated on demand rather than held: a shape is a few hundred triangles
/// of trigonometry, which is cheaper to rebuild than to keep alive for a
/// project that may never place one.
[[nodiscard]] MeshData makeEditorShapeMesh(EditorShapeKind kind);

/// Append the built-in shapes to @p assets and return the index the first
/// one took.
///
/// They are assets like any other, which is the whole point: a placement
/// names one by the same index it names a scanned model by, and everything
/// downstream — the transform, the marker, the properties panel, the scene
/// pass — needs to know nothing about where the geometry came from. What
/// marks them is `EditorAsset::shape`, which is what says to generate the
/// mesh instead of reading a file.
size_t appendEditorShapeAssets(std::vector<EditorAsset>& assets);

/// Whether a placement of @p asset starts solid. A model from a file does —
/// most of what is dropped is a crate or a wall — and a shape does as
/// `editorShapeCollides` says, which is every shape but the flat tile.
[[nodiscard]] bool editorAssetCollidesWhenPlaced(const EditorAsset& asset);

}  // namespace eng::editor
