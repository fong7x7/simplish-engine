#pragma once

/// @file build-temp-dir.h
/// @brief A scratch project folder a test builds into, removed after.
/// @par Threading Main-thread-only.

#include <filesystem>
#include <string>

namespace eng::editor::test {

/// A fresh, empty directory under the system's temporary folder, removed
/// with everything in it when this goes.
class BuildTempDir {
public:
  /// A directory named for @p label.
  explicit BuildTempDir(const std::string& label);
  ~BuildTempDir();
  BuildTempDir(const BuildTempDir&) = delete;
  BuildTempDir& operator=(const BuildTempDir&) = delete;
  BuildTempDir(BuildTempDir&&) = delete;
  BuildTempDir& operator=(BuildTempDir&&) = delete;

  /// The directory.
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  /// The directory.
  std::filesystem::path path_;
};

}  // namespace eng::editor::test
