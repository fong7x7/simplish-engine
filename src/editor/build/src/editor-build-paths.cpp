#include <editor/build/editor-build-paths.h>
#include <editor/project/project-paths.h>
#include <string>

namespace eng::editor {

namespace {

  /// This platform's extension for a library loaded at run time.
  constexpr std::string_view LIBRARY_EXTENSION =
#ifdef _WIN32
      ".dll";
#elifdef __APPLE__
      ".dylib";
#else
      ".so";
#endif

  /// This platform's extension for an executable.
  constexpr std::string_view EXECUTABLE_EXTENSION =
#ifdef _WIN32
      ".exe";
#else
      {};
#endif

  /// @p stem with the library extension.
  std::string libraryFileName(std::string_view stem) {
    return std::string(stem) + std::string(LIBRARY_EXTENSION);
  }

}  // namespace

std::filesystem::path
projectLogicBuildPath(const std::filesystem::path& root) {
  return projectBuildPath(root) / "logic";
}

std::filesystem::path
projectLogicLibraryPath(const std::filesystem::path& root) {
  return projectLogicBuildPath(root) / libraryFileName(LOGIC_LIBRARY_STEM);
}

std::filesystem::path projectLogicLoadPath(const std::filesystem::path& root,
                                           uint32_t generation) {
  return projectLogicBuildPath(root) / "loaded" /
         libraryFileName(std::string(LOGIC_LIBRARY_STEM) + "-" +
                         std::to_string(generation));
}

std::filesystem::path
projectDeployBuildPath(const std::filesystem::path& root) {
  return projectBuildPath(root) / "deploy-cmake";
}

std::filesystem::path projectDeployPath(const std::filesystem::path& root) {
  return projectBuildPath(root) / "deploy";
}

std::filesystem::path
deployBuiltExecutablePath(const std::filesystem::path& root) {
  return projectDeployBuildPath(root) / "src" / "bin" / "game" /
         executableFileName(DEPLOYED_GAME_STEM);
}

std::filesystem::path executableFileName(std::string_view name) {
  return std::string(name) + std::string(EXECUTABLE_EXTENSION);
}

std::filesystem::path projectBuildLogPath(const std::filesystem::path& root,
                                          EditorBuildKind kind) {
  return projectBuildPath(root) /
         (kind == EditorBuildKind::LOGIC ? "logic.log" : "deploy.log");
}

}  // namespace eng::editor
