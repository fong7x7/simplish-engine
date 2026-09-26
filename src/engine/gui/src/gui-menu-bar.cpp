#include <engine/gui/gui-button.h>
#include <engine/gui/gui-dropdown.h>
#include <engine/gui/gui-menu-bar.h>
#include <utility>

namespace eng {

namespace {

  /// A title's padding, and the space between titles.
  constexpr float TITLE_PAD_X = 10.0f;
  constexpr float TITLE_PAD_Y = 4.0f;
  constexpr float BAR_PAD = 6.0f;

  /// @p items with each row closing @p bar before it does its work.
  std::vector<GuiDropdownItem> closingItems(std::vector<GuiDropdownItem> items,
                                            GuiMenuBar& bar,
                                            GuiWidgetTree& tree) {
    for (GuiDropdownItem& item : items) {
      item.on_select = [work = item.on_select, &bar, &tree] {
        bar.close(tree);
        if (work) {
          work();
        }
      };
    }
    return items;
  }

  /// A hidden dropdown of @p items, placed by hand, over the scrim.
  std::unique_ptr<GuiDropdown> makeMenu(std::vector<GuiDropdownItem> items) {
    auto menu = std::make_unique<GuiDropdown>();
    menu->visible = false;
    menu->z_index = 1;
    menu->tree_layout.position = PositionMode::MANUAL;
    menu->items = std::move(items);
    return menu;
  }

}  // namespace

GuiMenuBar::GuiMenuBar() {
  debug_name = "gui-menu-bar";
  tree_layout.direction = FlexDirection::ROW;
  tree_layout.align_items = Align::CENTER;
  tree_layout.padding = {0.0f, BAR_PAD, 0.0f, BAR_PAD};
  tree_layout.gap = 2.0f;
}

std::unique_ptr<GuiWidget> GuiMenuBar::clone() const {
  return std::make_unique<GuiMenuBar>(*this);
}

void GuiMenuBar::render(const GuiDrawContext& ctx) const {
  const GuiPalette& p = ctx.activeTheme().palette;
  ctx.drawRect({.rect = rect,
                .fill = GuiColor::applyOpacity(p.surface, opacity),
                .border = {0.0f, 0.0f, 1.0f, 0.0f},
                .border_color = GuiColor::applyOpacity(p.border, opacity)});
}

void GuiMenuBar::build(GuiWidgetTree& tree) {
  unbuild(tree);
  buildScrim(tree);
  for (std::size_t i = 0; i < menus.size(); ++i) {
    buildMenu(tree, i);
  }
}

void GuiMenuBar::unbuild(GuiWidgetTree& tree) {
  for (const auto& ids : {titles_, dropdowns_}) {
    for (const GuiWidgetId id : ids) {
      tree.destroyWidget(id);
    }
  }
  tree.destroyWidget(scrim_);
  titles_.clear();
  dropdowns_.clear();
  scrim_ = GUI_WIDGET_ID_INVALID;
  open_ = -1;
}

void GuiMenuBar::buildMenu(GuiWidgetTree& tree, std::size_t index) {
  const GuiWidgetId title = tree.createWidget(GuiWidgetType::BUTTON, widget_id);
  auto& button = *dynamic_cast<GuiButton*>(tree.findWidget(title));
  button.label = menus[index].title;
  button.variant = GuiButtonVariant::GHOST;
  button.tree_layout.padding = {TITLE_PAD_Y, TITLE_PAD_X, TITLE_PAD_Y,
                                TITLE_PAD_X};
  const auto slot = static_cast<int>(index);
  button.onClick([this, &tree, slot](const GuiMouseEvent&) {
    open_ == slot ? close(tree) : open(tree, slot);
  });
  titles_.push_back(title);
  dropdowns_.push_back(tree.insertExternalWidget(
      makeMenu(closingItems(menus[index].items, *this, tree)),
      tree.overlayLayer()));
}

void GuiMenuBar::buildScrim(GuiWidgetTree& tree) {
  scrim_ = tree.createWidget(GuiWidgetType::PANEL, tree.overlayLayer());
  auto& scrim = *dynamic_cast<GuiPanel*>(tree.findWidget(scrim_));
  scrim.fill_color = GuiColor{0, 0, 0, 0};
  scrim.visible = false;
  scrim.tree_layout.position = PositionMode::ABSOLUTE;
  scrim.tree_layout.abs_right = 0.0f;
  scrim.tree_layout.abs_bottom = 0.0f;
  scrim.rect = tree.findWidget(tree.overlayLayer())->rect;
  scrim.onClick([this, &tree](const GuiMouseEvent& e) {
    // A click on a title through the scrim is the title's; elsewhere it
    // closes the menu.
    close(tree);
    followPointer(tree, e.x, e.y);
  });
  // The scrim covers the titles while a menu is open, so it is what sees
  // the pointer cross them.
  scrim.onMouseMove(
      [this, &tree](const GuiMouseEvent& e) { followPointer(tree, e.x, e.y); });
}

void GuiMenuBar::open(GuiWidgetTree& tree, int index) {
  close(tree);
  if (index < 0 || std::cmp_greater_equal(index, dropdowns_.size())) {
    return;
  }
  const auto i = static_cast<std::size_t>(index);
  const Rect viewport = tree.findWidget(tree.overlayLayer())->rect;
  auto& menu = *dynamic_cast<GuiDropdown*>(tree.findWidget(dropdowns_[i]));
  menu.popUp(tree.findWidget(titles_[i])->rect, viewport,
             {.side = GuiPopoverSide::BELOW, .gap = 2.0f});
  tree.findWidget(titles_[i])->selected = true;
  tree.findWidget(scrim_)->visible = true;
  open_ = index;
}

void GuiMenuBar::close(GuiWidgetTree& tree) {
  if (open_ >= 0) {
    const auto i = static_cast<std::size_t>(open_);
    tree.findWidget(dropdowns_[i])->visible = false;
    tree.findWidget(titles_[i])->selected = false;
  }
  if (GuiWidget* scrim = tree.findWidget(scrim_)) {
    scrim->visible = false;
  }
  open_ = -1;
}

void GuiMenuBar::followPointer(GuiWidgetTree& tree, float x, float y) {
  for (std::size_t i = 0; i < titles_.size(); ++i) {
    const GuiWidget* title = tree.findWidget(titles_[i]);
    if (title != nullptr && containsPoint(title->rect, x, y) &&
        std::cmp_not_equal(i, open_)) {
      open(tree, static_cast<int>(i));
      return;
    }
  }
}

int GuiMenuBar::openIndex() const {
  return open_;
}

}  // namespace eng
