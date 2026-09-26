#pragma once

/// @file deployed-content-fixture.h
/// @brief A deployed game's content in a temporary folder, for tests.
/// @par Threading Main-thread-only (writes the disk).

#include <cstdint>
#include <editor/deploy/deployed-game-options.h>
#include <filesystem>
#include <string>

namespace eng::editor::test {

/// A deployed game's content in a fresh temporary folder: one level,
/// `arena`, with every seat's start baked and one idle hostile actor.
/// Removed again when this goes.
class DeployedContentFixture {
public:
  /// The content, with @p extra written into a data file of its own so
  /// two fixtures can differ.
  explicit DeployedContentFixture(const std::string& extra = "");
  ~DeployedContentFixture();
  DeployedContentFixture(const DeployedContentFixture&) = delete;
  DeployedContentFixture& operator=(const DeployedContentFixture&) = delete;
  DeployedContentFixture(DeployedContentFixture&&) = delete;
  DeployedContentFixture& operator=(DeployedContentFixture&&) = delete;

  /// Options that run this content for @p ticks.
  [[nodiscard]] DeployedGameOptions options(uint64_t ticks) const;

  /// The folder.
  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  /// The folder.
  std::filesystem::path path_;
};

}  // namespace eng::editor::test
