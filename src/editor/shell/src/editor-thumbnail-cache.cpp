#include <cctype>
#include <cstdint>
#include <editor/shell/editor-thumbnail-cache.h>
#include <engine/gui/gui-software-rasterizer.h>
#include <engine/gui/image-loader.h>
#include <string>

namespace eng::editor {

namespace {

  namespace fs = std::filesystem;

  constexpr uint64_t FNV_OFFSET = 1469598103934665603ULL;
  constexpr uint64_t FNV_PRIME = 1099511628211ULL;
  /// Characters kept as-is in a cache file's readable half.
  constexpr std::string_view SAFE_EXTRA = "._-";

  /// FNV-1a, which is short, stable across runs and machines, and only
  /// being asked to name a file. Two assets collide only if they share a
  /// stem *and* a 64-bit hash.
  uint64_t hashPath(const std::string& text) {
    uint64_t hash = FNV_OFFSET;
    for (const char c : text) {
      hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
      hash *= FNV_PRIME;
    }
    return hash;
  }

  /// The asset's own name, reduced to what is safe in a file name, so the
  /// cache directory can be read by a person looking into it.
  std::string readableStem(const fs::path& relative) {
    std::string stem = relative.stem().string();
    for (char& c : stem) {
      const bool safe = (std::isalnum(static_cast<unsigned char>(c)) != 0) ||
                        SAFE_EXTRA.find(c) != std::string_view::npos;
      if (!safe) {
        c = '_';
      }
    }
    return stem.empty() ? "asset" : stem;
  }

  /// Last write time, or nullopt when the file cannot be examined.
  std::optional<fs::file_time_type> writeTime(const fs::path& path) {
    std::error_code ec;
    const fs::file_time_type time = fs::last_write_time(path, ec);
    if (ec) {
      return std::nullopt;
    }
    return time;
  }

  /// Whether the cached file is at least as new as the mesh it came from.
  bool isCurrent(const fs::path& cached, const fs::path& source) {
    const std::optional<fs::file_time_type> cache_time = writeTime(cached);
    const std::optional<fs::file_time_type> source_time = writeTime(source);
    if (!cache_time.has_value() || !source_time.has_value()) {
      return false;
    }
    return *cache_time >= *source_time;
  }

}  // namespace

fs::path thumbnailCacheFile(const ThumbnailCacheEntry& entry) {
  const std::string name =
      readableStem(entry.relative) + "-" +
      std::to_string(hashPath(entry.relative.generic_string())) + ".png";
  return entry.cache_dir / name;
}

std::optional<ImageData> loadCachedThumbnail(const ThumbnailCacheEntry& entry) {
  const fs::path cached = thumbnailCacheFile(entry);
  if (!isCurrent(cached, entry.source)) {
    return std::nullopt;
  }
  std::optional<ImageData> image = ImageLoader::loadFromFile(cached.string());
  if (!image.has_value() || image->width != ASSET_THUMBNAIL_SIZE ||
      image->height != ASSET_THUMBNAIL_SIZE) {
    return std::nullopt;
  }
  return image;
}

bool storeCachedThumbnail(const ThumbnailCacheEntry& entry,
                          const ImageData& image) {
  if (image.pixels.empty()) {
    return false;
  }
  // writePng creates the directories it needs, and reports its own failure
  // to do so. The PNG encoder happens to live on the software rasterizer,
  // which is the one place in the engine that had to write one before now.
  return GuiSoftwareRasterizer::writePng(image,
                                         thumbnailCacheFile(entry).string());
}

}  // namespace eng::editor
