#include <engine/gui/gui-render-transform.h>

namespace eng {

float GuiRenderTransform::mapX(float x) const {
  return x * scale + offset_x;
}

float GuiRenderTransform::mapY(float y) const {
  return y * scale + offset_y;
}

Rect GuiRenderTransform::map(const Rect& rect) const {
  return {mapX(rect.x), mapY(rect.y), rect.w * scale, rect.h * scale};
}

GuiRenderTransform
GuiRenderTransform::after(const GuiRenderTransform& inner) const {
  return {scale * inner.scale, mapX(inner.offset_x), mapY(inner.offset_y)};
}

GuiRenderTransform GuiRenderTransform::about(const Rect& box, float factor,
                                             float dx, float dy) {
  const float cx = box.x + box.w * 0.5f;
  const float cy = box.y + box.h * 0.5f;
  return {factor, cx - cx * factor + dx, cy - cy * factor + dy};
}

}  // namespace eng
