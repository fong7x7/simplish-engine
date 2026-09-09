#pragma once

/// @file editor-asset.h
/// @brief One importable asset the editor's asset panel lists.
/// @par Threading Main-thread-only.

#include <cstdint>
#include <editor/shell/editor-shape-kind.h>
#include <engine/math/vec3.h>
#include <engine/render-mesh/mesh-instance.h>
#include <engine/render/rhi-core-types.h>
#include <filesystem>
#include <optional>
#include <string>

namespace eng::editor {

/// How far an asset's card picture has got.
///
/// Colocated with the asset it belongs to: it is not a state anything else
/// in the editor has an opinion about.
/// @thread_safety Immutable value type.
enum class EditorAssetThumbnailState : uint8_t {
  /// Nothing has been attempted yet.
  PENDING,
  /// Uploaded, and the card draws it.
  READY,
  /// Could not be made. Remembered so it is not attempted every frame.
  FAILED,
};

/// A model the browser can place, and its GPU mesh once something has.
///
/// Usually a file the scan turned up. The built-in shapes are the other
/// sort: they carry a `shape` rather than a path, and their geometry is
/// generated where a file's would be read. Everything past that point
/// treats the two alike, which is why a placement can name either by one
/// index.
///
/// The mesh is uploaded lazily: scanning a directory should cost a
/// `directory_iterator` pass, not a parse and a GPU allocation per file,
/// and most assets in a project are never placed in any one session.
/// @thread_safety Main-thread-only.
struct EditorAsset {
  /// Display name, taken from the file stem, or the shape's own name.
  std::string name;
  /// Absolute path to the source file.
  std::filesystem::path path;
  /// Path relative to the project's assets directory, file name included
  /// (`props/crate.obj`). This is what identifies the asset for grouping
  /// under a folder, and it is stable across machines in a way `path` is
  /// not.
  std::filesystem::path relative_path;
  /// Which built-in shape this is, or nothing for a model on disk. It is
  /// what decides whether the mesh is generated or read, so the two paths
  /// above are empty exactly when this is set.
  ///
  /// Last of the fields that say what the asset is, so that the three a
  /// scan fills in stay the first three: the scan and its tests build these
  /// positionally.
  std::optional<EditorShapeKind> shape{};
  /// Stable identifier, as a level file references this asset by:
  /// `props_crate` for a model on disk, `cube` for a built-in shape. Never
  /// the asset's position in any list — see `editor-entity-id.h` for why
  /// that number cannot be an identity, and `assignEditorAssetIds` for
  /// where this is filled in.
  ///
  /// After `shape` so the positional construction above keeps working.
  std::string id{};
  /// Uploaded mesh, or `MESH_GPU_INVALID` until first placed.
  MeshGpuId mesh = MESH_GPU_INVALID;
  /// Diffuse map this model's material names, uploaded alongside the mesh,
  /// or `RHI_TEXTURE_INVALID` when it names none or the image would not
  /// load. Owned by the editor, which destroys it on rescan.
  ///
  /// Loaded with the mesh rather than lazily on its own: the material is
  /// read out of the same file, and a model that has reached the GPU
  /// without its map would draw the wrong thing for a frame or forever.
  RhiTextureHandle texture = RHI_TEXTURE_INVALID;
  /// Minimum bounds corner, in world orientation. Valid once uploaded.
  Vec3 min{};
  /// Maximum bounds corner, in world orientation. Valid once uploaded.
  Vec3 max{};
  /// Set when a load was attempted and failed, so it is not retried on
  /// every drop and the panel can show the asset as unusable.
  bool load_failed = false;
  /// Texture holding the card's picture, or `RHI_TEXTURE_INVALID` until one
  /// has been made. Owned by the editor, which destroys it on rescan.
  RhiTextureHandle thumbnail = RHI_TEXTURE_INVALID;
  /// How far that picture has got.
  EditorAssetThumbnailState thumbnail_state =
      EditorAssetThumbnailState::PENDING;
};

}  // namespace eng::editor
