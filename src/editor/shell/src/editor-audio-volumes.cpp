#include <editor/project/project-text-file.h>
#include <editor/shell/editor-audio-volumes.h>
#include <engine/audio/audio-volumes-json.h>
#include <engine/core/logger.h>
#include <string>
#include <system_error>

namespace eng::editor {

namespace {

  /// The settings @p text, read from @p file, describes, with whatever it
  /// got wrong logged.
  audio::AudioVolumes parseVolumes(const std::filesystem::path& file,
                                   const std::string& text) {
    audio::AudioVolumesLoad load = audio::parseAudioVolumes(text);
    for (const std::string& problem : load.problems) {
      LOG_WARN("editor", file.filename().string() + ": " + problem);
    }
    return load.volumes;
  }

}  // namespace

audio::AudioVolumes loadEditorAudioVolumes(const std::filesystem::path& file) {
  std::error_code error;
  if (file.empty()) {
    return {};
  }
  if (!std::filesystem::exists(file, error)) {
    (void)saveEditorAudioVolumes(file, {});
    return {};
  }
  if (const std::optional<std::string> text = readProjectTextFile(file)) {
    return parseVolumes(file, *text);
  }
  LOG_WARN("editor", "Could not read " + file.string() + "; full volume");
  return {};
}

bool saveEditorAudioVolumes(const std::filesystem::path& file,
                            const audio::AudioVolumes& volumes) {
  if (file.empty()) {
    return false;
  }
  if (!writeProjectTextFile(file, audio::writeAudioVolumes(volumes))) {
    LOG_WARN("editor",
             "Could not write the volume settings to " + file.string());
    return false;
  }
  return true;
}

}  // namespace eng::editor
