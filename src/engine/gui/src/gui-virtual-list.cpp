#include <algorithm>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-virtual-list.h>
#include <utility>

namespace eng {

namespace {

  /// Pixels a wheel notch scrolls, and the scroll thumb's size.
  constexpr float WHEEL_STEP = 48.0f;
  constexpr float THUMB_W = 4.0f;
  constexpr float THUMB_MIN = 16.0f;

  /// The background a row is drawn on, or nothing.
  GuiColor rowFill(const GuiListRow& row, const GuiPalette& p) {
    if (row.selected) {
      return GuiColor::applyOpacity(p.primary, 0.35f);
    }
    return row.hovered ? GuiColor::applyOpacity(p.control_hover, 0.6f)
                       : GuiColor{0, 0, 0, 0};
  }

  /// Pushes a clip for its lifetime when there is a renderer.
  class ListClip {
  public:
    ListClip(const GuiDrawContext& ctx, const Rect& clip)
      : renderer_(ctx.renderer) {
      if (renderer_ != nullptr) {
        renderer_->pushScissor(clip);
      }
    }
    ~ListClip() {
      if (renderer_ != nullptr) {
        renderer_->popScissor();
      }
    }
    ListClip(const ListClip&) = delete;
    ListClip& operator=(const ListClip&) = delete;
    ListClip(ListClip&&) = delete;
    ListClip& operator=(ListClip&&) = delete;

  private:
    /// The renderer clipped, or null.
    GuiRendererContext* renderer_ = nullptr;
  };

}  // namespace

GuiVirtualList::GuiVirtualList() {
  tree_focusable = true;
}

std::unique_ptr<GuiWidget> GuiVirtualList::clone() const {
  return std::make_unique<GuiVirtualList>(*this);
}

Rect GuiVirtualList::bodyRect() const {
  return rect;
}

float GuiVirtualList::scrollOffset() const {
  return scroll_;
}

void GuiVirtualList::render(const GuiDrawContext& ctx) const {
  const Rect body = bodyRect();
  {
    const ListClip clip{ctx, body};
    drawRows(ctx, body);
  }
  const float content = row_height * static_cast<float>(row_count);
  if (content <= body.h || content <= 0.0f) {
    return;
  }
  const float len = std::max(THUMB_MIN, body.h * body.h / content);
  const float at = body.y + (body.h - len) * scroll_ / (content - body.h);
  ctx.drawRoundedRect(
      {body.x + body.w - THUMB_W - 2.0f, at, THUMB_W, len},
      GuiColor::applyOpacity(ctx.activeTheme().palette.text_muted, opacity),
      THUMB_W * 0.5f);
}

void GuiVirtualList::drawRows(const GuiDrawContext& ctx,
                              const Rect& body) const {
  if (row_height <= 0.0f) {
    return;
  }
  const auto first = static_cast<std::size_t>(scroll_ / row_height);
  const auto shown = static_cast<std::size_t>(body.h / row_height) + 2;
  for (std::size_t i = first; i < std::min(row_count, first + shown); ++i) {
    drawRow(ctx,
            {i,
             {body.x, body.y + static_cast<float>(i) * row_height - scroll_,
              body.w, row_height},
             isSelected(i),
             std::cmp_equal(i, hovered_row_),
             focused && i == current_});
  }
}

void GuiVirtualList::drawRow(const GuiDrawContext& ctx,
                             const GuiListRow& row) const {
  const GuiTheme& t = ctx.activeTheme();
  ctx.drawRoundedRect(row.rect,
                      GuiColor::applyOpacity(rowFill(row, t.palette), opacity),
                      t.radius(GuiRadius::SM));
  if (draw_row) {
    draw_row(ctx, row);
  }
}

LayoutSize GuiVirtualList::measureContent(const GuiDrawContext& /*ctx*/,
                                          float /*max_width*/) const {
  return {};
}

long GuiVirtualList::rowAt(float x, float y) const {
  const Rect body = bodyRect();
  if (!containsPoint(body, x, y) || row_height <= 0.0f) {
    return -1;
  }
  const auto row = static_cast<long>((y - body.y + scroll_) / row_height);
  return std::cmp_less(row, row_count) ? row : -1;
}

bool GuiVirtualList::handleClick(const GuiMouseEvent& event) {
  const long row = rowAt(event.x, event.y);
  if (disabled || row < 0) {
    return false;
  }
  clickRow(static_cast<std::size_t>(row), event);
  if (event.type == GuiMouseEventType::DOUBLE_CLICK && on_activate) {
    on_activate(static_cast<std::size_t>(row));
  }
  return true;
}

void GuiVirtualList::clickRow(std::size_t index, const GuiMouseEvent& event) {
  current_ = index;
  if (selection_mode == GuiListSelection::MULTIPLE && event.shift_held) {
    selected.clear();
    for (std::size_t i = std::min(anchor_, index);
         i <= std::max(anchor_, index); ++i) {
      selected.push_back(i);
    }
  } else if (selection_mode != GuiListSelection::NONE) {
    selected = {index};
    anchor_ = index;
  }
  if (on_select && selection_mode != GuiListSelection::NONE) {
    on_select(selected);
  }
}

void GuiVirtualList::handleMouseMove(const GuiMouseEvent& event) {
  hovered_row_ = rowAt(event.x, event.y);
  GuiWidget::handleMouseMove(event);
}

bool GuiVirtualList::handleScroll(const GuiScrollEvent& event) {
  return scrollBy(0.0f, -event.delta_y * WHEEL_STEP);
}

bool GuiVirtualList::scrollBy(float /*dx*/, float dy) {
  const float was = scroll_;
  scroll_ += dy;
  clampScroll();
  return scroll_ != was;
}

void GuiVirtualList::clampScroll() {
  const float content = row_height * static_cast<float>(row_count);
  scroll_ = std::clamp(scroll_, 0.0f, std::max(0.0f, content - bodyRect().h));
}

bool GuiVirtualList::handleNav(GuiNavCommand command) {
  if (disabled || row_count == 0) {
    return false;
  }
  if (command == GuiNavCommand::DOWN && current_ + 1 < row_count) {
    setCurrent(current_ + 1);
    return true;
  }
  if (command == GuiNavCommand::UP && current_ > 0) {
    setCurrent(current_ - 1);
    return true;
  }
  if (command == GuiNavCommand::CONFIRM && on_activate) {
    on_activate(current_);
    return true;
  }
  return false;
}

void GuiVirtualList::setCurrent(std::size_t index) {
  if (index >= row_count) {
    return;
  }
  current_ = index;
  anchor_ = index;
  if (selection_mode != GuiListSelection::NONE) {
    selected = {index};
    if (on_select) {
      on_select(selected);
    }
  }
  reveal(index);
}

void GuiVirtualList::reveal(std::size_t index) {
  const float top = static_cast<float>(index) * row_height;
  const float h = bodyRect().h;
  if (top < scroll_) {
    scroll_ = top;
  } else if (top + row_height > scroll_ + h) {
    scroll_ = top + row_height - h;
  }
  clampScroll();
}

bool GuiVirtualList::isSelected(std::size_t index) const {
  return std::ranges::binary_search(selected, index);
}

}  // namespace eng
