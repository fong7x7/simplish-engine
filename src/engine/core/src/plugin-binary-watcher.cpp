#include <chrono>
#include <engine/core/logger.h>
#include <engine/core/plugin-binary-watcher.h>
#include <filesystem>

namespace eng {

namespace {

  constexpr std::string_view LOG_TAG = "PluginBinaryWatcher";

  /// Get the platform-specific plugin path suffix from a LoadedPlugin.
  /// Returns empty string if the plugin has no binary path set.
  std::string resolveBinaryPath(std::string_view mods_dir,
                                const LoadedPlugin& plugin) {
    if (plugin.plugin_path.empty()) {
      return {};
    }
    auto path =
        std::filesystem::path(mods_dir) / plugin.mod_id / plugin.plugin_path;
    return path.string();
  }

}  // namespace

int64_t PluginBinaryWatcher::getFileMtimeNs(std::string_view path) {
  std::error_code ec;
  auto fs_path = std::filesystem::path(path);
  if (!std::filesystem::exists(fs_path, ec)) {
    return 0;
  }
  auto mtime = std::filesystem::last_write_time(fs_path, ec);
  if (ec) {
    return 0;
  }
  auto duration = mtime.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

PluginBinaryWatcher
PluginBinaryWatcher::create(const PluginBinaryWatcher::Config& config,
                            const std::vector<LoadedPlugin>& plugins) {
  PluginBinaryWatcher watcher;
  for (const auto& plugin : plugins) {
    auto path = resolveBinaryPath(config.mods_directory, plugin);
    if (path.empty()) {
      continue;
    }
    PluginBinaryWatcher::WatchedBinary entry;
    entry.mod_id = plugin.mod_id;
    entry.binary_path = std::move(path);
    entry.last_mtime_ns = getFileMtimeNs(entry.binary_path);
    watcher.watched_binaries.push_back(std::move(entry));
  }
  Logger::info(LOG_TAG, "watching " +
                            std::to_string(watcher.watched_binaries.size()) +
                            " plugin binaries");
  return watcher;
}

std::vector<std::string>
PluginBinaryWatcher::poll(PluginBinaryWatcher& watcher) {
  std::vector<std::string> changed_mod_ids;
  for (auto& entry : watcher.watched_binaries) {
    int64_t current_mtime = getFileMtimeNs(entry.binary_path);
    if (current_mtime == 0) {
      continue;
    }
    if (current_mtime != entry.last_mtime_ns) {
      changed_mod_ids.push_back(entry.mod_id);
      entry.last_mtime_ns = current_mtime;
    }
  }
  return changed_mod_ids;
}

}  // namespace eng
