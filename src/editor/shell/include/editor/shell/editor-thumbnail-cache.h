#pragma once

/// @file editor-thumbnail-cache.h
/// @brief Keeping generated thumbnails in the project between sessions.
/// @par Threading Main-thread-only (reads and writes the filesystem).

#include <editor/shell/editor-asset-thumbnail.h>
#include <engine/gui/image-data.h>
#include <filesystem>
#include <optional>

namespace eng::editor {

/// One asset's place in a project's thumbnail cache.
/// @thread_safety Immutable value type.
struct ThumbnailCacheEntry {
  /// Directory the project keeps its thumbnails in.
  std::filesystem::path cache_dir;
  /// Absolute path to the source mesh, which is what is examined to decide
  /// whether a cached picture is still current.
  std::filesystem::path source;
  /// Path relative to the assets root, which names the cache file. Naming
  /// by this rather than by the absolute path means a project keeps its
  /// thumbnails when it is moved or cloned somewhere else.
  std::filesystem::path relative;
};

/// Where @p entry's thumbnail is kept. Exposed for tests and for anything
/// that later wants to sweep the directory; callers use load and store.
[[nodiscard]] std::filesystem::path
thumbnailCacheFile(const ThumbnailCacheEntry& entry);

/// Read the cached thumbnail for @p entry, or nullopt when there is none.
///
/// Freshness is the cache file's own modification time against the source
/// mesh's, which is how make has always done it: no stamp to keep in step
/// with the file, and re-rendering writes the file that records it.
///
/// A cached picture of the wrong size counts as a miss, so changing
/// `ASSET_THUMBNAIL_SIZE` retires every cache in every project by itself. A
/// corrupt or truncated file is a miss too, rather than an error: the one
/// thing the caller can do about it is render the thumbnail again.
[[nodiscard]] std::optional<ImageData>
loadCachedThumbnail(const ThumbnailCacheEntry& entry);

/// Write @p image as @p entry's cached thumbnail, creating the cache
/// directory if it is not there yet.
///
/// False when it could not be written, which is not fatal: the thumbnail is
/// already rendered and usable, it just will not outlive the session.
bool storeCachedThumbnail(const ThumbnailCacheEntry& entry,
                          const ImageData& image);

}  // namespace eng::editor
