#include <algorithm>
#include <engine/core/dynamic-library.h>
#include <engine/core/logger.h>
#include <engine/core/plugin-binary-validation.h>
#include <engine/core/plugin-hot-reload.h>
#include <engine/core/plugin-manager.h>
#include <filesystem>
#include <ranges>

namespace eng {

namespace {

  constexpr std::string_view LOG_TAG = "PluginManager";

  /// Resolve the absolute path to a plugin binary.
  std::string resolvePath(std::string_view mods_dir,
                          const CoreModManifest& manifest) {
    if (manifest.plugin_path.empty()) {
      return {};
    }
    auto path =
        std::filesystem::path(mods_dir) / manifest.id / manifest.plugin_path;
    return path.string();
  }

  LoadedPlugin makeBasePlugin(const CoreModManifest& manifest) {
    LoadedPlugin plugin{};
    plugin.mod_id = manifest.id;
    plugin.plugin_name = manifest.name;
    plugin.plugin_path = manifest.plugin_path;
    return plugin;
  }

  bool initPluginBinary(LoadedPlugin& plugin, PluginAPI* api) {
    using InitFn = bool (*)(PluginAPI*);
    auto* init_fn = reinterpret_cast<
        InitFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(plugin.dlhandle, "simplishPluginInit"));
    if (init_fn != nullptr && init_fn(api)) {
      return true;
    }
    Logger::error(LOG_TAG, "init failed for " + plugin.mod_id);
    DynamicLibrary::close(plugin.dlhandle);
    plugin.dlhandle = nullptr;
    plugin.is_healthy.store(false);
    return false;
  }

  /// Validate and attach the binary to the plugin, returning false on failure.
  bool attachBinary(LoadedPlugin& plugin, const std::string& binary_path,
                    const std::string& mod_id) {
    auto validation =
        validatePluginBinary(binary_path, EXPECTED_PLUGIN_API_VERSION);
    if (validation.dlhandle == nullptr) {
      Logger::error(LOG_TAG, "failed to load " + mod_id + ": " +
                                 validation.error_message);
      plugin.is_healthy.store(false);
      return false;
    }
    plugin.dlhandle = validation.dlhandle;
    return true;
  }

  /// Load a single plugin: validate binary, call init, populate LoadedPlugin.
  LoadedPlugin loadSinglePlugin(const CoreModManifest& manifest,
                                std::string_view mods_dir, PluginAPI* api) {
    auto plugin = makeBasePlugin(manifest);
    auto binary_path = resolvePath(mods_dir, manifest);
    if (binary_path.empty()) {
      Logger::info(LOG_TAG, "data-only mod: " + manifest.id);
      return plugin;
    }
    if (!attachBinary(plugin, binary_path, manifest.id)) {
      return plugin;
    }
    if (initPluginBinary(plugin, api)) {
      Logger::info(LOG_TAG, "loaded plugin: " + manifest.id);
    }
    return plugin;
  }

  /// Call shutdown and close handle for a single plugin.
  void unloadSinglePlugin(LoadedPlugin& plugin) {
    if (plugin.dlhandle == nullptr) {
      return;
    }
    using ShutdownFn = void (*)();
    auto* fn = reinterpret_cast<
        ShutdownFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                      // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(plugin.dlhandle, "simplishPluginShutdown"));
    if (fn != nullptr) {
      fn();
    }
    DynamicLibrary::close(plugin.dlhandle);
    plugin.dlhandle = nullptr;
    Logger::info(LOG_TAG, "unloaded plugin: " + plugin.mod_id);
  }

}  // namespace

std::optional<std::vector<LoadedPlugin>>
PluginManager::loadMods(std::string_view mods_dir,
                        const std::vector<CoreModManifest>& manifests,
                        PluginAPI* game_plugin_api) {
  if (!manifests.empty() && !std::filesystem::exists(mods_dir)) {
    return std::nullopt;
  }
  loaded_plugins_.clear();
  for (const auto& manifest : manifests) {
    loaded_plugins_.push_back(
        loadSinglePlugin(manifest, mods_dir, game_plugin_api));
  }
  return std::move(loaded_plugins_);
}

void PluginManager::stopAsyncTickLoops() {
  Logger::info(LOG_TAG, "stopping async tick loops");
}

void PluginManager::unloadPlugins() {
  Logger::info(LOG_TAG, "unloading " + std::to_string(loaded_plugins_.size()) +
                            " plugins");
  for (auto& plugin : std::ranges::reverse_view(loaded_plugins_)) {
    unloadSinglePlugin(plugin);
  }
  loaded_plugins_.clear();
}

const std::vector<LoadedPlugin>& PluginManager::plugins() const {
  return loaded_plugins_;
}

std::vector<LoadedPlugin>& PluginManager::mutablePlugins() {
  return loaded_plugins_;
}

void PluginManager::markUnhealthy(std::string_view mod_id) {
  auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
      loaded_plugins_.begin(), loaded_plugins_.end(),
      [&](const LoadedPlugin& p) { return p.mod_id == mod_id; });
  if (it != loaded_plugins_.end()) {
    it->is_healthy.store(false);
  }
}

}  // namespace eng
