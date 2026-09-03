#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <engine/gui/gui-font-discovery.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eng;

namespace {

/// A temp directory removed when the test scope exits, so a failing
/// assertion cannot leave state behind for the next run.
class TempDir {
public:
  explicit TempDir(const std::string& label) {
    path_ = fs::temp_directory_path() /
            ("simplish-gui-test-" + label + "-" +
             std::to_string(reinterpret_cast<uintptr_t>(this)));
    std::error_code ec;
    fs::remove_all(path_, ec);
    fs::create_directories(path_, ec);
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

  /// Create an empty file with the given name. Discovery reads names only,
  /// so these stand in for fonts everywhere except an actual load.
  void touch(const std::string& name) const {
    std::ofstream out(path_ / name);
    out << "not a real font";
  }

private:
  fs::path path_;
};

/// True when `fonts` contains an entry with this family name.
bool hasFamily(const std::vector<GuiFontListEntry>& fonts,
               std::string_view family) {
  for (const auto& font : fonts) {
    if (font.family == family) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("the platform has at least one system font directory") {
  REQUIRE_FALSE(guiSystemFontDirectories().empty());
}

#ifdef __APPLE__
TEST_CASE("macOS searches the directory its own UI fonts live in") {
  // /Library/Fonts is often close to empty on a clean install; the faces the
  // OS actually renders with are under /System/Library/Fonts. Missing it is
  // what left the editor with no font to load.
  const auto dirs = guiSystemFontDirectories();
  bool found = false;
  for (const auto& dir : dirs) {
    found = found || dir == fs::path("/System/Library/Fonts");
  }
  REQUIRE(found);
}
#endif

TEST_CASE("discovery accepts ttf, otf, and ttc") {
  TempDir tmp("ext");
  tmp.touch("Alpha.ttf");
  tmp.touch("Beta.otf");
  // A TrueType collection: FreeType opens face 0 of one exactly like a .ttf,
  // and on macOS most system faces ship only in that form.
  tmp.touch("Gamma.ttc");
  tmp.touch("Delta.txt");

  const auto fonts = discoverGuiFonts(tmp.path());
  REQUIRE(hasFamily(fonts, "Alpha"));
  REQUIRE(hasFamily(fonts, "Beta"));
  REQUIRE(hasFamily(fonts, "Gamma"));
  REQUIRE_FALSE(hasFamily(fonts, "Delta"));
}

TEST_CASE("fonts under the bundled directory are marked bundled") {
  TempDir tmp("bundled");
  tmp.touch("Alpha.ttf");

  const auto fonts = discoverGuiFonts(tmp.path());
  REQUIRE_FALSE(fonts.empty());
  REQUIRE(fonts.front().family == "Alpha");
  REQUIRE(fonts.front().is_bundled);
}

TEST_CASE("selectGuiUiFont prefers a bundled font over any system font") {
  TempDir tmp("select");
  tmp.touch("ProjectSans.ttf");

  const auto chosen = selectGuiUiFont(tmp.path());
  REQUIRE(chosen.has_value());
  REQUIRE(chosen->family == "ProjectSans");
  REQUIRE(chosen->is_bundled);
}

TEST_CASE("selectGuiUiFont falls back to a system font") {
  // No bundled directory: the machine's own fonts have to answer.
  const auto chosen = selectGuiUiFont({});
  if (!chosen.has_value()) {
    SKIP("no fonts installed on this machine");
  }
  REQUIRE_FALSE(chosen->file_path.empty());
  REQUIRE_FALSE(chosen->is_bundled);
}

TEST_CASE("the preferred-family list is non-empty on every platform") {
  REQUIRE_FALSE(guiPreferredUiFontFamilies().empty());
}
