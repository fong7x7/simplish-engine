#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <editor/project/project-paths.h>
#include <editor/shell/editor-thumbnail-cache.h>
#include <engine/render-mesh/mesh-data.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using namespace eng::editor;

namespace {

/// A temp directory removed when the test scope exits.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-thumbs-" + label + "-" +
             std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_ / "assets", ec);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;
  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  [[nodiscard]] const fs::path& path() const { return path_; }

  /// Write a stub source mesh at a path relative to the assets root.
  fs::path writeAsset(const std::string& relative) const {
    const fs::path file = path_ / "assets" / relative;
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);
    std::ofstream out(file);
    out << "v 0 0 0\n";
    return file;
  }

private:
  fs::path path_;
};

/// A solid image of the size the cache expects.
eng::ImageData makeImage(uint8_t red) {
  eng::ImageData image;
  image.width = ASSET_THUMBNAIL_SIZE;
  image.height = ASSET_THUMBNAIL_SIZE;
  image.source_channels = 4;
  image.pixels.assign(static_cast<size_t>(ASSET_THUMBNAIL_SIZE) *
                          ASSET_THUMBNAIL_SIZE * 4,
                      255);
  for (size_t i = 0; i < image.pixels.size(); i += 4) {
    image.pixels[i] = red;
  }
  return image;
}

/// An entry for an asset at a relative path under a temp project.
ThumbnailCacheEntry entryFor(const TempDir& tmp, const std::string& relative) {
  return {projectThumbnailsPath(tmp.path()), tmp.writeAsset(relative),
          fs::path(relative)};
}

/// Age a file, so it reads as older than something written after it.
///
/// Aging the cache rather than advancing the source: a source stamped in
/// the future is a broken clock, not an edit, and no thumbnail can ever
/// look current beside one. Waiting on the real clock would make this slow.
void ageFile(const fs::path& path) {
  std::error_code ec;
  const fs::file_time_type time = fs::last_write_time(path, ec);
  fs::last_write_time(path, time - std::chrono::seconds(30), ec);
}

}  // namespace

TEST_CASE("an empty cache misses") {
  const TempDir tmp("miss");

  REQUIRE_FALSE(loadCachedThumbnail(entryFor(tmp, "crate.obj")).has_value());
}

TEST_CASE("a stored thumbnail is found again") {
  const TempDir tmp("hit");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");

  REQUIRE(storeCachedThumbnail(entry, makeImage(200)));
  const std::optional<eng::ImageData> loaded = loadCachedThumbnail(entry);
  REQUIRE(loaded.has_value());
  REQUIRE(loaded->width == ASSET_THUMBNAIL_SIZE);
  REQUIRE(loaded->pixels[0] == 200);
}

TEST_CASE("storing creates the cache directory") {
  const TempDir tmp("mkdir");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  REQUIRE_FALSE(fs::exists(entry.cache_dir));

  REQUIRE(storeCachedThumbnail(entry, makeImage(10)));
  REQUIRE(fs::is_directory(entry.cache_dir));
}

TEST_CASE("the cache lives under the project's editor directory") {
  const TempDir tmp("location");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  storeCachedThumbnail(entry, makeImage(10));

  // Derived files, not authored ones: a project can be committed without
  // them, which only holds if they are not sitting beside the assets.
  REQUIRE(thumbnailCacheFile(entry).string().find(".simplish") !=
          std::string::npos);
  REQUIRE(thumbnailCacheFile(entry).extension() == ".png");
}

TEST_CASE("a thumbnail older than its source is retired") {
  const TempDir tmp("stale");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  REQUIRE(storeCachedThumbnail(entry, makeImage(200)));
  REQUIRE(loadCachedThumbnail(entry).has_value());

  // What editing the mesh leaves behind: a picture of what it used to be.
  ageFile(thumbnailCacheFile(entry));

  REQUIRE_FALSE(loadCachedThumbnail(entry).has_value());
}

TEST_CASE("re-storing makes a retired thumbnail current again") {
  const TempDir tmp("restore");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  storeCachedThumbnail(entry, makeImage(200));
  ageFile(thumbnailCacheFile(entry));
  REQUIRE_FALSE(loadCachedThumbnail(entry).has_value());

  REQUIRE(storeCachedThumbnail(entry, makeImage(50)));
  const std::optional<eng::ImageData> loaded = loadCachedThumbnail(entry);
  REQUIRE(loaded.has_value());
  REQUIRE(loaded->pixels[0] == 50);
}

TEST_CASE("re-storing overwrites rather than piling up") {
  const TempDir tmp("overwrite");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");

  storeCachedThumbnail(entry, makeImage(200));
  ageFile(thumbnailCacheFile(entry));
  storeCachedThumbnail(entry, makeImage(50));

  // One cache file per asset, whatever happens to the asset: the name
  // depends on where the file is, not on what is in it.
  size_t files = 0;
  for (const auto& file : fs::directory_iterator(entry.cache_dir)) {
    files += file.is_regular_file() ? 1 : 0;
  }
  REQUIRE(files == 1);
}

TEST_CASE("a missing source has no current thumbnail") {
  const TempDir tmp("gone");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  storeCachedThumbnail(entry, makeImage(200));

  std::error_code ec;
  fs::remove(entry.source, ec);

  REQUIRE_FALSE(loadCachedThumbnail(entry).has_value());
}

TEST_CASE("a corrupt cache file is a miss, not a crash") {
  const TempDir tmp("corrupt");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  storeCachedThumbnail(entry, makeImage(200));
  {
    std::ofstream out(thumbnailCacheFile(entry), std::ios::binary);
    out << "this is not a png";
  }

  // Nothing the caller can do about it but render the thumbnail again.
  REQUIRE_FALSE(loadCachedThumbnail(entry).has_value());
}

TEST_CASE("a thumbnail of the wrong size is a miss") {
  const TempDir tmp("resize");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  eng::ImageData small = makeImage(200);
  small.width = ASSET_THUMBNAIL_SIZE / 2;
  small.height = ASSET_THUMBNAIL_SIZE / 2;
  small.pixels.resize(static_cast<size_t>(small.width) * small.height * 4, 255);

  REQUIRE(storeCachedThumbnail(entry, small));

  // Changing ASSET_THUMBNAIL_SIZE has to retire every cache in every
  // project, and this is what does it.
  REQUIRE_FALSE(loadCachedThumbnail(entry).has_value());
}

TEST_CASE("an empty image is not stored") {
  const TempDir tmp("empty");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");

  // A mesh that would not render has no picture worth remembering.
  REQUIRE_FALSE(storeCachedThumbnail(entry, eng::ImageData{}));
  REQUIRE_FALSE(fs::exists(thumbnailCacheFile(entry)));
}

TEST_CASE("assets in different folders get different cache files") {
  const TempDir tmp("distinct");
  const ThumbnailCacheEntry root = entryFor(tmp, "crate.obj");
  const ThumbnailCacheEntry nested = entryFor(tmp, "props/crate.obj");

  // Same stem, different folders — exactly the case the flat strip could
  // not tell apart.
  REQUIRE(thumbnailCacheFile(root) != thumbnailCacheFile(nested));

  storeCachedThumbnail(root, makeImage(10));
  storeCachedThumbnail(nested, makeImage(90));
  REQUIRE(loadCachedThumbnail(root)->pixels[0] == 10);
  REQUIRE(loadCachedThumbnail(nested)->pixels[0] == 90);
}

TEST_CASE("the cache file is named after the asset") {
  const TempDir tmp("readable");
  const ThumbnailCacheEntry entry = entryFor(tmp, "props/stone_wall.obj");

  // Readable on purpose: someone looking into the directory should be able
  // to tell what is in it.
  REQUIRE(
      thumbnailCacheFile(entry).filename().string().starts_with("stone_wall-"));
}

TEST_CASE("the cache file does not depend on where the project sits") {
  const TempDir first("moved-a");
  const TempDir second("moved-b");
  const ThumbnailCacheEntry a = entryFor(first, "props/crate.obj");
  const ThumbnailCacheEntry b = entryFor(second, "props/crate.obj");

  // Naming by the relative path is what lets a project keep its thumbnails
  // when it is moved or cloned somewhere else.
  REQUIRE(thumbnailCacheFile(a).filename() == thumbnailCacheFile(b).filename());
}

TEST_CASE("a rendered thumbnail survives the round trip through the cache") {
  const TempDir tmp("roundtrip");
  const ThumbnailCacheEntry entry = entryFor(tmp, "crate.obj");
  eng::MeshData mesh;
  const eng::Vec3 normal{0.0f, -1.0f, 0.0f};
  mesh.vertices.push_back({{-1.0f, 0.0f, 0.0f}, normal});
  mesh.vertices.push_back({{1.0f, 0.0f, 0.0f}, normal});
  mesh.vertices.push_back({{1.0f, 0.0f, 2.0f}, normal});
  mesh.indices = {0, 1, 2};
  mesh.min = {-1.0f, 0.0f, 0.0f};
  mesh.max = {1.0f, 0.0f, 2.0f};

  const eng::ImageData rendered =
      renderAssetThumbnail(mesh, ASSET_THUMBNAIL_SIZE, ISO_AXES_DIMETRIC);
  REQUIRE(storeCachedThumbnail(entry, rendered));

  // PNG is lossless and both ends are RGBA8, so what comes back is what
  // went in — which is what lets a cached card look identical to a freshly
  // rendered one.
  const std::optional<eng::ImageData> loaded = loadCachedThumbnail(entry);
  REQUIRE(loaded.has_value());
  REQUIRE(loaded->pixels == rendered.pixels);
}
