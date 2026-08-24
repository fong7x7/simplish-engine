#include <engine/core/hot-reload-result.h>

namespace eng {

const char* hotReloadResultToString(HotReloadResult result) {
  switch (result) {
    case HotReloadResult::SUCCESS:
      return "success";
    case HotReloadResult::BINARY_NOT_FOUND:
      return "binary not found";
    case HotReloadResult::DLOPEN_FAILED:
      return "dlopen failed";
    case HotReloadResult::MISSING_SYMBOLS:
      return "missing required symbols";
    case HotReloadResult::API_VERSION_MISMATCH:
      return "API version mismatch";
    case HotReloadResult::INIT_FAILED:
      return "plugin init failed";
  }
  return "unknown";
}

}  // namespace eng
