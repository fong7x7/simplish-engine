#include <engine/core/dynamic-library.h>
#include <engine/core/logger.h>
#include <engine/core/plugin-binary-validation.h>
#include <filesystem>

namespace eng {

namespace {

  constexpr std::string_view LOG_TAG = "PluginValidation";

  /// Resolve a required symbol or set error message on failure.
  void* resolveRequired(void* handle, std::string_view name,
                        std::string& error_out) {
    void* sym = DynamicLibrary::symbol(handle, name);
    if (sym == nullptr && error_out.empty()) {
      error_out = "missing symbol: " + std::string(name);
    }
    return sym;
  }

  using VersionFn = uint32_t (*)();

  void resolveAllSymbols(PluginBinaryValidation& result) {
    resolveRequired(result.dlhandle, "simplishPluginInit",
                    result.error_message);
    resolveRequired(result.dlhandle, "simplishPluginShutdown",
                    result.error_message);
    resolveRequired(result.dlhandle, "simplishPluginName",
                    result.error_message);
    resolveRequired(result.dlhandle, "simplishPluginVersion",
                    result.error_message);
  }

  VersionFn resolveVersionFn(PluginBinaryValidation& result) {
    return reinterpret_cast<
        VersionFn>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                     // — C ABI function pointer from dlsym
        resolveRequired(result.dlhandle, "simplishPluginApiVersion",
                        result.error_message));
  }

  bool checkApiVersion(PluginBinaryValidation& result, VersionFn version_fn,
                       uint32_t expected) {
    result.api_version = version_fn();
    if (result.api_version == expected) {
      return true;
    }
    result.error_message = "API version mismatch: expected " +
                           std::to_string(expected) + ", got " +
                           std::to_string(result.api_version);
    return false;
  }

  /// Open and resolve symbols from the plugin binary.
  PluginBinaryValidation openAndResolve(std::string_view binary_path) {
    PluginBinaryValidation result{};
    if (!std::filesystem::exists(binary_path)) {
      result.error_message = "binary not found: " + std::string(binary_path);
      return result;
    }
    result.dlhandle = DynamicLibrary::open(binary_path);
    if (result.dlhandle == nullptr) {
      result.error_message = "dlopen failed: " + DynamicLibrary::lastError();
      return result;
    }
    resolveAllSymbols(result);
    return result;
  }

}  // namespace

PluginBinaryValidation validatePluginBinary(std::string_view binary_path,
                                            uint32_t expected_api_version) {
  auto result = openAndResolve(binary_path);
  auto* version_fn = resolveVersionFn(result);
  if (!result.error_message.empty()) {
    closeValidatedBinary(result);
    return result;
  }
  if (!checkApiVersion(result, version_fn, expected_api_version)) {
    closeValidatedBinary(result);
    return result;
  }
  Logger::info(LOG_TAG, "validated binary: " + std::string(binary_path) +
                            " (API v" + std::to_string(result.api_version) +
                            ")");
  return result;
}

void closeValidatedBinary(PluginBinaryValidation& validation) {
  if (validation.dlhandle != nullptr) {
    DynamicLibrary::close(validation.dlhandle);
    validation.dlhandle = nullptr;
  }
}

}  // namespace eng
