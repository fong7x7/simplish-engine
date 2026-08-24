#include <engine/core/plugin-context.h>

namespace eng {

void PluginContext::initNullableDomains(PluginAPI& api) {
  if (api.render != nullptr) {
    render_.emplace(*api.render);
  }
  if (api.audio != nullptr) {
    audio_.emplace(*api.audio);
  }
  if (api.input != nullptr) {
    input_.emplace(*api.input);
  }
}

}  // namespace eng
