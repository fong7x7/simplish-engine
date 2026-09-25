#include <editor/build/editor-build-paths.h>
#include <editor/build/editor-logic-source.h>
#include <editor/build/editor-project-guide.h>
#include <editor/build/editor-toolchain.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <system_error>

namespace eng::editor {

namespace {

  /// The newest write time of any file under @p dir, or nothing when it
  /// holds none that can be read.
  std::optional<std::filesystem::file_time_type>
  newestWrite(const std::filesystem::path& dir) {
    std::error_code ec;
    std::optional<std::filesystem::file_time_type> newest;
    std::filesystem::recursive_directory_iterator it(dir, ec);
    for (; !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
      std::error_code time_ec;
      const auto written = std::filesystem::last_write_time(*it, time_ec);
      if (!time_ec && it->is_regular_file(time_ec) &&
          (!newest || written > *newest)) {
        newest = written;
      }
    }
    return newest;
  }

  /// Ignore everything in `build/`, from a `.gitignore` inside it.
  bool ignoreBuildFolder(const std::filesystem::path& root) {
    return writeProjectTextFile(projectBuildPath(root) / ".gitignore",
                                "# Everything here is built by the editor.\n*\n");
  }

}  // namespace

bool projectHasLogic(const std::filesystem::path& root) {
  std::error_code ec;
  return std::filesystem::is_regular_file(
      projectSourcePath(root) / LOGIC_CMAKE_FILE_NAME, ec);
}

EditorLogicScaffold scaffoldProjectLogic(const std::filesystem::path& root) {
  if (projectHasLogic(root)) {
    return EditorLogicScaffold::ALREADY_THERE;
  }
  const std::filesystem::path src = projectSourcePath(root);
  const bool written =
      writeProjectTextFile(src / LOGIC_EXAMPLE_FILE_NAME,
                           logicScaffoldSource()) &&
      writeProjectTextFile(src / LOGIC_CMAKE_FILE_NAME, logicScaffoldCMake()) &&
      ignoreBuildFolder(root) &&
      writeProjectAgentGuide(root, editorToolchain().engine_root);
  return written ? EditorLogicScaffold::CREATED : EditorLogicScaffold::FAILED;
}

bool projectLogicStale(const std::filesystem::path& root) {
  const auto source = newestWrite(projectSourcePath(root));
  if (!source) {
    return false;
  }
  std::error_code ec;
  const auto built =
      std::filesystem::last_write_time(projectLogicLibraryPath(root), ec);
  return ec || *source > built;
}

}  // namespace eng::editor
