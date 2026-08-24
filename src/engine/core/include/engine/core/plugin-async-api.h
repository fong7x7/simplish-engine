#pragma once

#include "plugin-api.h"

#include <string>
#include <string_view>

namespace eng {

/// Type-safe C++ wrapper around the C ABI async task submission API.
/// Thread Safety: main-thread-only (same as PluginAPI contract).
class AsyncApi {
public:
  AsyncApi(PluginAPI& api, std::string_view plugin_id)
    : api_(&api), plugin_id_(plugin_id) {}

  TaskID submit(void (*work_fn)(void*, void*, size_t), void* user_data) {
    return api_->async_submit(plugin_id_.c_str(), work_fn, user_data);
  }

  /// Parameters for consuming async task results.
  struct ConsumeResultsParams {
    /// Output buffer for completed task IDs.
    TaskID* out_ids;
    /// Output buffer for result data pointers.
    void** out_results;
    /// Output buffer for result data sizes.
    size_t* out_sizes;
    /// Maximum number of results to consume.
    uint32_t max_results;
  };

  uint32_t consumeResults(const ConsumeResultsParams& p) {
    return api_->async_consume_results(plugin_id_.c_str(), p.out_ids,
                                       p.out_results, p.out_sizes,
                                       p.max_results);
  }

  uint32_t pendingCount() const {
    return api_->async_get_pending_count(plugin_id_.c_str());
  }

  bool cancel(TaskID task_id) {
    return api_->async_cancel(plugin_id_.c_str(), task_id);
  }

  /// Register a dedicated async tick loop at the given Hz rate.
  /// Returns true if registration succeeded.
  bool registerAsyncTick(void (*tick_fn)(float delta_seconds, void* user_data),
                         void* user_data, float tick_rate_hz) {
    return api_->register_async_tick(plugin_id_.c_str(), tick_fn, user_data,
                                     tick_rate_hz);
  }

  /// Unregister this plugin's async tick loop.
  void unregisterAsyncTick() {
    api_->unregister_async_tick(plugin_id_.c_str());
  }

private:
  /// Raw C ABI plugin API pointer for async task submission.
  PluginAPI* api_;
  /// Unique identifier of the owning plugin.
  std::string plugin_id_;
};

}  // namespace eng
