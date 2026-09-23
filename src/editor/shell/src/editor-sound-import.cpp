#include <editor/shell/editor-asset-scan.h>
#include <editor/shell/editor-sound-import.h>
#include <engine/audio/audio-decode.h>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  namespace fs = std::filesystem;

  /// @p source relative to @p assets_dir, when it is inside it.
  std::optional<fs::path> insideAssets(const fs::path& assets_dir,
                                       const fs::path& source) {
    std::error_code error;
    const fs::path root = fs::weakly_canonical(assets_dir, error);
    const fs::path file = fs::weakly_canonical(source, error);
    fs::path relative = file.lexically_relative(root);
    if (error || relative.empty() || *relative.begin() == "..") {
      return std::nullopt;
    }
    return relative;
  }

  /// A name for @p source in @p folder that no file there has yet.
  fs::path freeName(const fs::path& folder, const fs::path& source) {
    std::error_code error;
    fs::path name = source.filename();
    for (int n = 2; fs::exists(folder / name, error); ++n) {
      name = source.stem().string() + "-" + std::to_string(n) +
             source.extension().string();
    }
    return name;
  }

  /// Copy @p source into `sounds/` under @p assets_dir.
  EditorSoundImport copyIn(const fs::path& assets_dir, const fs::path& source) {
    const fs::path folder = assets_dir / EDITOR_IMPORTED_SOUNDS_DIR;
    std::error_code error;
    fs::create_directories(folder, error);
    const fs::path name = freeName(folder, source);
    fs::copy_file(source, folder / name, error);
    if (error) {
      return {.error = "could not copy " + source.filename().string() +
                       " into the project: " + error.message()};
    }
    return {.file = fs::path(EDITOR_IMPORTED_SOUNDS_DIR) / name};
  }

}  // namespace

EditorSoundImport importEditorSound(const fs::path& assets_dir,
                                    const fs::path& source) {
  if (!isSoundFile(source)) {
    return {.error = source.filename().string() +
                     " is not a sound file (.wav or .ogg)"};
  }
  if (!audio::loadAudioFile(source)) {
    return {.error = source.filename().string() +
                     " is missing, or not a WAV or Ogg Vorbis file the "
                     "engine can play"};
  }
  if (const std::optional<fs::path> inside = insideAssets(assets_dir, source)) {
    return {.file = *inside};
  }
  return copyIn(assets_dir, source);
}

}  // namespace eng::editor
