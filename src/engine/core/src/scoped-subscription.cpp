#include <engine/core/scoped-subscription.h>

namespace eng {

ScopedSubscription::ScopedSubscription(ScopedSubscription&& other) noexcept
  : handle_(other.handle_), api_(other.api_) {
  other.handle_ = 0;
  other.api_ = nullptr;
}

ScopedSubscription&
ScopedSubscription::operator=(ScopedSubscription&& other) noexcept {
  if (this != &other) {
    if (api_ != nullptr && handle_ != 0) {
      api_->unsubscribe_event(handle_);
    }
    handle_ = other.handle_;
    api_ = other.api_;
    other.handle_ = 0;
    other.api_ = nullptr;
  }
  return *this;
}

SubscriptionHandle ScopedSubscription::release() {
  auto h = handle_;
  handle_ = 0;
  api_ = nullptr;
  return h;
}

}  // namespace eng
