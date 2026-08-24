#include <engine/core/loaded-plugin.h>

namespace eng {

LoadedPlugin& LoadedPlugin::operator=(LoadedPlugin&& other) noexcept {
  mod_id = std::move(other.mod_id);
  plugin_name = std::move(other.plugin_name);
  plugin_path = std::move(other.plugin_path);
  dlhandle = other.dlhandle;
  has_async_tick = other.has_async_tick;
  async_tick_rate_hz = other.async_tick_rate_hz;
  is_healthy.store(other.is_healthy.load());
  other.dlhandle = nullptr;
  return *this;
}

}  // namespace eng
