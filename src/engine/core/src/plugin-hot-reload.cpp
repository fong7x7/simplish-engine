#include <algorithm>
#include <engine/core/dynamic-library.h>
#include <engine/core/logger.h>
#include <engine/core/plugin-binary-validation.h>
#include <engine/core/plugin-hot-reload.h>
#include <filesystem>
#include <vector>

namespace eng {

namespace {

  constexpr std::string_view LOG_TAG = "PluginHotReload";

  /// Find a loaded plugin by mod_id. Returns nullptr if not found.
  LoadedPlugin* findPlugin(std::vector<LoadedPlugin>& plugins,
                           std::string_view mod_id) {
    auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
        plugins.begin(), plugins.end(),
        [&](const LoadedPlugin& p) { return p.mod_id == mod_id; });
    return it != plugins.end() ? &(*it) : nullptr;
  }

  /// Call simplishPluginShutdown on the old binary if the symbol exists.
  void callShutdown(void* dlhandle) {
    auto* fn = reinterpret_cast<
        void (*)()>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                      // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(dlhandle, "simplishPluginShutdown"));
    if (fn != nullptr) {
      fn();
    }
  }

  /// Try to serialize plugin state. Returns buffer (empty if not supported).
  std::vector<uint8_t> trySerializeState(void* dlhandle) {
    using SerializeFn = size_t (*)(void*, size_t);
    auto* fn = reinterpret_cast<
        SerializeFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                       // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(dlhandle, "simplishPluginSerializeState"));
    if (fn == nullptr) {
      return {};
    }
    size_t needed = fn(nullptr, 0);
    if (needed == 0) {
      return {};
    }
    std::vector<uint8_t> buffer(needed);
    fn(buffer.data(), buffer.size());
    Logger::info(LOG_TAG, "serialized " + std::to_string(needed) +
                              " bytes of plugin state");
    return buffer;
  }

  /// Try to deserialize plugin state into the new binary.
  void tryDeserializeState(void* dlhandle, const std::vector<uint8_t>& buffer) {
    if (buffer.empty()) {
      return;
    }
    using DeserializeFn = bool (*)(const void*, size_t);
    auto* fn = reinterpret_cast<
        DeserializeFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                         // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(dlhandle, "simplishPluginDeserializeState"));
    if (fn == nullptr) {
      Logger::info(LOG_TAG, "new binary has no deserialize symbol; "
                            "starting fresh");
      return;
    }
    if (!fn(buffer.data(), buffer.size())) {
      Logger::warn(LOG_TAG, "deserialize returned false; plugin starts "
                            "with fresh state");
    }
  }

  /// Call simplishPluginInit on the new binary.
  bool callInit(void* dlhandle, PluginAPI* api) {
    using InitFn = bool (*)(PluginAPI*);
    auto* fn = reinterpret_cast<
        InitFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                  // — C ABI function pointer from dlsym
        DynamicLibrary::symbol(dlhandle, "simplishPluginInit"));
    if (fn == nullptr) {
      return false;
    }
    return fn(api);
  }

}  // namespace

// Algorithm: Hot-reload a plugin by serializing old state, shutting down,
// loading the new binary, initializing, and restoring state.
HotReloadResult hotReloadPlugin(const PluginHotReloadContext& ctx,
                                PluginManager& manager) {
  Logger::info(LOG_TAG, "hot-reloading plugin: " + std::string(ctx.mod_id));
  auto& plugins = manager.mutablePlugins();
  LoadedPlugin* plugin = findPlugin(plugins, ctx.mod_id);
  if (plugin == nullptr) {
    Logger::error(LOG_TAG, "plugin not found: " + std::string(ctx.mod_id));
    return HotReloadResult::BINARY_NOT_FOUND;
  }
  // Serialize state, shutdown, and close old binary
  auto state_buffer = trySerializeState(plugin->dlhandle);
  callShutdown(plugin->dlhandle);
  DynamicLibrary::close(plugin->dlhandle);
  plugin->dlhandle = nullptr;
  // Validate and open new binary
  auto validation =
      validatePluginBinary(ctx.binary_path, EXPECTED_PLUGIN_API_VERSION);
  if (validation.dlhandle == nullptr) {
    Logger::error(LOG_TAG, "reload failed for " + std::string(ctx.mod_id) +
                               ": " + validation.error_message);
    manager.markUnhealthy(ctx.mod_id);
    return HotReloadResult::DLOPEN_FAILED;
  }
  // Init new binary
  if (!callInit(validation.dlhandle, ctx.plugin_api)) {
    Logger::error(LOG_TAG, "init failed for " + std::string(ctx.mod_id));
    closeValidatedBinary(validation);
    manager.markUnhealthy(ctx.mod_id);
    return HotReloadResult::INIT_FAILED;
  }
  // Deserialize state and update record
  tryDeserializeState(validation.dlhandle, state_buffer);
  plugin->dlhandle = validation.dlhandle;
  plugin->is_healthy.store(true);
  Logger::info(LOG_TAG, "hot-reload succeeded for " + std::string(ctx.mod_id));
  return HotReloadResult::SUCCESS;
}

}  // namespace eng
