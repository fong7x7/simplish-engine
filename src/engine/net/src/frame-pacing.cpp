#include <algorithm>
#include <engine/net/frame-pacing.h>

namespace eng::net {

std::size_t framesToStep(uint32_t due, std::size_t waiting) {
  if (waiting <= due) {
    return waiting;
  }
  const std::size_t backlog = waiting - due;
  const std::size_t catch_up =
      (backlog + NET_CATCH_UP_DIVISOR - 1) / NET_CATCH_UP_DIVISOR;
  return std::min(waiting, due + catch_up);
}

}  // namespace eng::net
