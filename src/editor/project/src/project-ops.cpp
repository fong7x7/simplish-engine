#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <editor/project/project-json-ops.h>
#include <editor/project/project-ops.h>
#include <editor/project/project-paths.h>
#include <editor/project/project-text-file.h>
#include <string>
#include <system_error>
#include <utility>

namespace eng::editor {

namespace {

  /// Validate that @p root is an existing directory. Returns NONE when it is.
  ProjectOpenError classifyRoot(const std::filesystem::path& root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || ec) {
      return ProjectOpenError::PATH_NOT_FOUND;
    }
    if (!std::filesystem::is_directory(root, ec) || ec) {
      return ProjectOpenError::NOT_A_DIRECTORY;
    }
    return ProjectOpenError::NONE;
  }

  ProjectOpenResult fail(ProjectOpenError error) {
    return {{}, error};
  }

  /// A successful result for the project rooted at @p root.
  ///
  /// The root is made absolute here, once, so that nothing downstream
  /// depends on the directory the editor happened to be launched from.
  ProjectOpenResult opened(const std::filesystem::path& root,
                           ProjectMetadata metadata) {
    ProjectOpenResult result;
    std::error_code ec;
    auto absolute = std::filesystem::absolute(root, ec);
    result.context.root = ec ? root : absolute;
    result.context.metadata = std::move(metadata);
    result.context.loaded = true;
    return result;
  }

  /// The manifest a project is created with.
  ProjectMetadata newProjectMetadata(std::string_view name,
                                     std::string_view timestamp) {
    ProjectMetadata metadata;
    metadata.name = std::string(name);
    metadata.engine_version = SIMPLISH_ENGINE_VERSION;
    metadata.created_at = std::string(timestamp);
    metadata.last_opened_at = std::string(timestamp);
    return metadata;
  }

  /// Create the directories a project is born with, so that a new project
  /// already has the shape project-format.md describes. Each is empty until
  /// something is put in it. NONE when every one of them is there.
  ProjectOpenError makeProjectDirectories(const std::filesystem::path& root) {
    const std::filesystem::path dirs[] = {root, projectDataPath(root),
                                          projectAssetsPath(root),
                                          projectLevelsPath(root)};
    std::error_code ec;
    for (const std::filesystem::path& dir : dirs) {
      std::filesystem::create_directories(dir, ec);
      if (ec) {
        return ProjectOpenError::WRITE_FAILED;
      }
    }
    return ProjectOpenError::NONE;
  }

}  // namespace

bool isProjectDirectory(const std::filesystem::path& root) {
  std::error_code ec;
  bool present = std::filesystem::is_regular_file(projectFilePath(root), ec);
  return present && !ec;
}

ProjectOpenResult openProject(const std::filesystem::path& root) {
  if (auto error = classifyRoot(root); error != ProjectOpenError::NONE) {
    return fail(error);
  }
  if (!isProjectDirectory(root)) {
    return fail(ProjectOpenError::NOT_A_PROJECT);
  }

  auto contents = readProjectTextFile(projectFilePath(root));
  if (!contents) {
    return fail(ProjectOpenError::UNREADABLE);
  }
  auto metadata = parseProjectMetadata(*contents);
  if (!metadata) {
    return fail(ProjectOpenError::MALFORMED);
  }

  return opened(root, std::move(*metadata));
}

ProjectOpenResult createProject(const std::filesystem::path& root,
                                std::string_view name,
                                std::string_view timestamp) {
  // Refuse to write over an existing project: creating is never a way to
  // discard someone else's manifest.
  if (isProjectDirectory(root)) {
    return fail(ProjectOpenError::ALREADY_EXISTS);
  }

  if (auto error = makeProjectDirectories(root);
      error != ProjectOpenError::NONE) {
    return fail(error);
  }

  ProjectMetadata metadata = newProjectMetadata(name, timestamp);
  if (!writeProjectTextFile(projectFilePath(root),
                            serializeProjectMetadata(metadata))) {
    return fail(ProjectOpenError::WRITE_FAILED);
  }
  return opened(root, std::move(metadata));
}

bool touchProjectOpened(ProjectContext& context, std::string_view timestamp) {
  if (!context.loaded) {
    return false;
  }
  context.metadata.last_opened_at = std::string(timestamp);
  return writeProjectTextFile(projectFilePath(context.root),
                              serializeProjectMetadata(context.metadata));
}

bool saveProjectMetadata(const ProjectContext& context) {
  if (!context.loaded) {
    return false;
  }
  return writeProjectTextFile(projectFilePath(context.root),
                              serializeProjectMetadata(context.metadata));
}

RecentProjectsList loadRecentProjects(const std::filesystem::path& path) {
  auto contents = readProjectTextFile(path);
  if (!contents) {
    // First run, or the file was removed. Both are ordinary states.
    return {};
  }
  auto parsed = parseRecentProjects(*contents);
  return parsed ? *parsed : RecentProjectsList{};
}

bool saveRecentProjects(const RecentProjectsList& list,
                        const std::filesystem::path& path) {
  return writeProjectTextFile(path, serializeRecentProjects(list));
}

void promoteRecentProject(RecentProjectsList& list,
                          const ProjectContext& context,
                          std::string_view timestamp) {
  if (!context.loaded) {
    return;
  }
  auto path = context.root.string();
  removeRecentProject(list, path);

  RecentProjectEntry entry;
  entry.path = std::move(path);
  entry.name = context.metadata.name;
  entry.last_opened_at = std::string(timestamp);
  list.entries.insert(list.entries.begin(), std::move(entry));

  if (list.entries.size() > RECENT_PROJECTS_MAX) {
    list.entries.resize(RECENT_PROJECTS_MAX);
  }
}

bool removeRecentProject(RecentProjectsList& list, std::string_view path) {
  auto match = [path](const RecentProjectEntry& entry) {
    return entry.path == path;
  };
  auto removed =
      std::remove_if(list.entries.begin(), list.entries.end(), match);
  if (removed == list.entries.end()) {
    return false;
  }
  list.entries.erase(removed, list.entries.end());
  return true;
}

std::string isoTimestampNow() {
  auto now = std::chrono::system_clock::now();
  std::time_t seconds = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &seconds);
#else
  gmtime_r(&seconds, &utc);
#endif
  std::array<char, 32> buffer{};
  std::snprintf(buffer.data(), buffer.size(), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
                utc.tm_min, utc.tm_sec);
  return {buffer.data()};
}

}  // namespace eng::editor
