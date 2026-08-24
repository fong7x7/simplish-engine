#include <engine/core/render-domain-wrapper.h>

namespace eng {

Vec3 RenderDomainWrapper::getCameraPos() const {
  Vec3 pos{};
  api_->get_camera_pos(&pos);
  return pos;
}

}  // namespace eng
