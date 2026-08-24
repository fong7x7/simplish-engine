#include <algorithm>
#include <chrono>
#include <engine/core/event-bus.h>
#include <thread>

namespace eng {

/// Nanoseconds per millisecond for timestamp conversion.
constexpr uint64_t NS_PER_MS = 1000000;

uint64_t EventBus::registerEvent(std::string_view event_name) {
  const uint64_t id = fnv1a(event_name);
  registered_events_.try_emplace(id, std::string(event_name));
  return id;
}

SubscriptionHandle EventBus::subscribe(uint64_t event_id,
                                       EventHandler handler) {
  const SubscriptionHandle handle = next_handle_++;
  subscriptions_[event_id].push_back({handle, handler});
  return handle;
}

bool EventBus::unsubscribe(SubscriptionHandle handle) {
  for (auto& [event_id, subs] : subscriptions_) {
    auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
        subs.begin(), subs.end(),
        [handle](const Subscription& s) { return s.handle == handle; });
    if (it != subs.end()) {
      subs.erase(it);
      return true;
    }
  }
  return false;
}

namespace {

  /// Build an EventContext for the current emission.
  EventContext makeEmitContext(uint64_t event_id, const void* payload,
                               size_t payload_size) {
    EventContext ctx{};
    ctx.event_id = event_id;
    ctx.timestamp_ms = static_cast<int64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count() /
        static_cast<int64_t>(NS_PER_MS));
    ctx.emitter_thread = std::this_thread::get_id();
    ctx.payload = payload;
    ctx.payload_size = payload_size;
    return ctx;
  }

}  // namespace

void EventBus::dispatchToSubscribers(const std::vector<Subscription>& subs,
                                     const EventContext& ctx) {
  for (const auto& sub : subs) {
    sub.handler(ctx);
  }
}

void EventBus::emit(uint64_t event_id, const void* payload,
                    size_t payload_size) {
  if (currently_emitting_event_id_.has_value() &&
      *currently_emitting_event_id_ == event_id) {
    return;
  }
  const std::optional<uint64_t> prev = currently_emitting_event_id_;
  currently_emitting_event_id_ = event_id;
  const EventContext ctx = makeEmitContext(event_id, payload, payload_size);
  for (const auto& trace : trace_subscribers_) {
    trace.handler(ctx);
  }
  auto it = subscriptions_.find(event_id);
  if (it != subscriptions_.end()) {
    dispatchToSubscribers(it->second, ctx);
  }
  currently_emitting_event_id_ = prev;
}

SubscriptionHandle EventBus::subscribeTrace(TraceHandler handler) {
  const SubscriptionHandle handle = next_handle_++;
  trace_subscribers_.push_back({handle, std::move(handler)});
  return handle;
}

bool EventBus::unsubscribeTrace(SubscriptionHandle handle) {
  auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
      trace_subscribers_.begin(), trace_subscribers_.end(),
      [handle](const TraceSubscription& s) { return s.handle == handle; });
  if (it == trace_subscribers_.end()) {
    return false;
  }
  trace_subscribers_.erase(it);
  return true;
}

size_t EventBus::traceHandlerCount() const {
  return trace_subscribers_.size();
}

std::unordered_map<uint64_t, std::string> EventBus::registeredEvents() const {
  return registered_events_;
}

size_t EventBus::handlerCount(uint64_t event_id) const {
  auto it = subscriptions_.find(event_id);
  if (it == subscriptions_.end()) {
    return 0;
  }
  return it->second.size();
}

}  // namespace eng
