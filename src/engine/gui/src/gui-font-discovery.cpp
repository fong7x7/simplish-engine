#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <engine/gui/gui-font-discovery.h>
#include <set>
#include <string>

namespace eng {
namespace {

  enum class FontSource { BUNDLED, SYSTEM };

  bool isFontFile(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    for (auto& c : ext) {
      auto u = static_cast<unsigned char>(c);
      c = static_cast<char>(std::tolower(u));
    }
    return ext == ".ttf" || ext == ".otf";
  }

  std::string familyFromPath(const std::filesystem::path& path) {
    return path.stem().string();
  }

  // Algorithm: Iterate directory; dedupe font families into out list.
  void scanDir(const std::filesystem::path& dir, FontSource source,
               std::vector<GuiFontListEntry>& out,
               std::set<std::string>& seen) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) {
      return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
      if (ec) {
        break;
      }
      if (!entry.is_regular_file(ec)) {
        continue;
      }
      if (!isFontFile(entry.path())) {
        continue;
      }
      auto family = familyFromPath(entry.path());
      if (seen.contains(family)) {
        continue;
      }
      seen.insert(family);
      const bool bundled = source == FontSource::BUNDLED;
      out.push_back({family, entry.path().string(), bundled});
    }
  }

}  // namespace

std::vector<std::filesystem::path> guiSystemFontDirectories() {
  std::vector<std::filesystem::path> dirs;

#ifdef __APPLE__
  dirs.emplace_back("/Library/Fonts");
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only; called at init
  if (const char* home = std::getenv("HOME")) {
    dirs.emplace_back(std::string(home) + "/Library/Fonts");
  }
#elifdef _WIN32
  dirs.emplace_back("C:\\Windows\\Fonts");
#else
  dirs.emplace_back("/usr/share/fonts");
  // NOLINTNEXTLINE(concurrency-mt-unsafe) -- main-thread-only; called at init
  if (const char* home = std::getenv("HOME")) {
    dirs.emplace_back(std::string(home) + "/.local/share/fonts");
  }
#endif

  return dirs;
}

std::vector<GuiFontListEntry>
discoverGuiFonts(const std::filesystem::path& bundled_dir) {
  std::vector<GuiFontListEntry> result;
  std::set<std::string> seen;

  scanDir(bundled_dir, FontSource::BUNDLED, result, seen);

  for (const auto& dir : guiSystemFontDirectories()) {
    scanDir(dir, FontSource::SYSTEM, result, seen);
  }

  return result;
}

}  // namespace eng
