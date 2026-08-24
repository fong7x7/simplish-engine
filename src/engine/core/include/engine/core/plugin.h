#pragma once

#include "plugin-api.h"
#include "plugin-context.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY — Plugin Base Class
// Technical Approach: docs/technical-approaches/engine/plugin-cpp-wrappers.md
//
// Behaviours:
//   - Abstract base class for C++ plugins; subclass and override lifecycle
//   - ENG_REGISTER_PLUGIN macro generates extern "C" entry points
//   - PluginContext provides typed domain accessors (plugin-context.h)
//
// Thread Safety: main-thread-only.
// ============================================================================

class Plugin {
public:
  virtual ~Plugin() = default;

  virtual bool onInit(PluginContext& ctx) = 0;

  virtual void onShutdown() = 0;

  virtual std::string_view name() const = 0;

  virtual std::string_view version() const = 0;

  virtual uint32_t apiVersion() const = 0;

  Plugin(const Plugin&) = delete;
  Plugin& operator=(const Plugin&) = delete;
  Plugin(Plugin&&) = delete;
  Plugin& operator=(Plugin&&) = delete;

protected:
  Plugin() = default;
};

}  // namespace eng

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

#define ENG_REGISTER_PLUGIN(PluginClass)                                       \
  namespace {                                                                  \
    alignas(PluginClass) char eng_plugin_storage_[sizeof(PluginClass)];        \
    ::eng::Plugin* eng_plugin_instance_ = nullptr;                             \
    std::unique_ptr<::eng::PluginContext> eng_plugin_ctx_;                     \
  }                                                                            \
                                                                               \
  extern "C" bool simplishPluginInit(::eng::PluginAPI* api) {                  \
    eng_plugin_instance_ =                                                     \
        new (eng_plugin_storage_) PluginClass(); /* NOLINT */                  \
    eng_plugin_ctx_ = std::make_unique<::eng::PluginContext>(                  \
        *api, eng_plugin_instance_->name());                                   \
    bool ok = eng_plugin_instance_->onInit(*eng_plugin_ctx_);                  \
    if (!ok) {                                                                 \
      eng_plugin_ctx_.reset();                                                 \
      eng_plugin_instance_->~Plugin();                                         \
      eng_plugin_instance_ = nullptr;                                          \
    }                                                                          \
    return ok;                                                                 \
  }                                                                            \
                                                                               \
  extern "C" void simplishPluginShutdown(void) {                               \
    if (eng_plugin_instance_ != nullptr) {                                     \
      eng_plugin_instance_->onShutdown();                                      \
      eng_plugin_ctx_.reset();                                                 \
      eng_plugin_instance_->~Plugin();                                         \
      eng_plugin_instance_ = nullptr;                                          \
    }                                                                          \
  }                                                                            \
                                                                               \
  extern "C" const char* simplishPluginName(void) {                            \
    static const std::string eng_cached_name_{PluginClass().name()};           \
    return eng_cached_name_.c_str();                                           \
  }                                                                            \
                                                                               \
  extern "C" const char* simplishPluginVersion(void) {                         \
    static const std::string eng_cached_version_{PluginClass().version()};     \
    return eng_cached_version_.c_str();                                        \
  }                                                                            \
                                                                               \
  extern "C" uint32_t simplishPluginApiVersion(void) {                         \
    return PluginClass().apiVersion();                                         \
  }

// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
