#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/gui-widget.h>
#include <utility>

namespace eng {

namespace {

  /// An animation of @p property from @p from to @p to, timed as @p how.
  GuiAnimation presenceAnim(GuiAnimProperty property, float from, float to,
                            const GuiPresence& how) {
    return {.property = property,
            .start = {.scalar = from},
            .target = {.scalar = to},
            .duration = how.seconds,
            .easing = how.easing};
  }

}  // namespace

void GuiWidget::enter(const GuiPresence& from) {
  opacity = from.opacity;
  render_scale = from.scale;
  render_offset_x = from.offset_x;
  render_offset_y = from.offset_y;
  animate(presenceAnim(GuiAnimProperty::OPACITY, from.opacity, 1.0f, from));
  animate(presenceAnim(GuiAnimProperty::RENDER_SCALE, from.scale, 1.0f, from));
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_X, from.offset_x, 0.0f,
                       from));
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_Y, from.offset_y, 0.0f,
                       from));
}

void GuiWidget::leave(const GuiPresence& to, std::function<void()> done) {
  animate(
      presenceAnim(GuiAnimProperty::RENDER_SCALE, render_scale, to.scale, to));
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_X, render_offset_x,
                       to.offset_x, to));
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_Y, render_offset_y,
                       to.offset_y, to));
  GuiAnimation fade =
      presenceAnim(GuiAnimProperty::OPACITY, opacity, to.opacity, to);
  fade.on_complete = std::move(done);
  animate(fade);
}

void GuiWidget::glideFrom(float dx, float dy) {
  const GuiPresence glide{.seconds = layout_glide,
                          .easing = layout_glide_easing};
  render_offset_x = dx;
  render_offset_y = dy;
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_X, dx, 0.0f, glide));
  animate(presenceAnim(GuiAnimProperty::RENDER_OFFSET_Y, dy, 0.0f, glide));
}

DrawPos GuiWidget::contentOrigin() const {
  return {rect.x, rect.y};
}

void GuiWidgetTree::glideMoved(GuiWidget& widget) {
  const GuiWidget* parent = findWidget(widget.parent_id);
  const DrawPos origin =
      parent != nullptr ? parent->contentOrigin() : DrawPos{0.0f, 0.0f};
  const DrawPos now{widget.rect.x - origin.x, widget.rect.y - origin.y};
  const std::optional<DrawPos> was = std::exchange(widget.laid_out_at, now);
  if (!was || widget.layout_glide <= 0.0f ||
      (was->x == now.x && was->y == now.y)) {
    return;
  }
  // From where it is drawn now — mid-glide, perhaps — to its new place.
  widget.glideFrom(was->x - now.x + widget.render_offset_x,
                   was->y - now.y + widget.render_offset_y);
}

void GuiWidgetTree::startGlides() {
  for (auto& entry : widget_nodes) {
    if (entry.second->layout_glide > 0.0f) {
      glideMoved(*entry.second);
    }
  }
}

void GuiWidgetTree::dismiss(GuiWidgetId id, const GuiPresence& to) {
  GuiWidget* widget = findWidget(id);
  if (widget == nullptr) {
    return;
  }
  widget->pointer_through = true;
  widget->leave(to, [this, id] { dismissed_.push_back(id); });
}

}  // namespace eng
