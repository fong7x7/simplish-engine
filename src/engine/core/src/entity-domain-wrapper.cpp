#include <engine/core/entity-domain-wrapper.h>

namespace eng {

std::optional<Vec3> EntityDomainWrapper::getPosition(EntityID id) const {
  Vec3 pos{};
  if (api_->get_position(id, &pos)) {
    return pos;
  }
  return std::nullopt;
}

}  // namespace eng
