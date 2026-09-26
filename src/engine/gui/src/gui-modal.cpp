#include <engine/gui/gui-card.h>
#include <engine/gui/gui-modal.h>
#include <engine/gui/gui-widget-tree.h>

namespace eng {

namespace {

  /// Padding inside a modal's card, and between its children.
  constexpr float CARD_PADDING = 20.0f;
  constexpr float CARD_GAP = 12.0f;

}  // namespace

GuiModal::GuiModal() {
  visible = false;
  debug_name = "gui-modal";
  tree_layout.position = PositionMode::ABSOLUTE;
  tree_layout.abs_right = 0.0f;
  tree_layout.abs_bottom = 0.0f;
  tree_layout.justify_content = Align::CENTER;
  tree_layout.align_items = Align::CENTER;
}

std::unique_ptr<GuiWidget> GuiModal::clone() const {
  return std::make_unique<GuiModal>(*this);
}

void GuiModal::render(const GuiDrawContext& ctx) const {
  ctx.drawFilledRect(
      rect, GuiColor::applyOpacity(ctx.activeTheme().palette.scrim, opacity));
}

bool GuiModal::handleMouseDown(const GuiMouseEvent& /*event*/) {
  return false;
}

bool GuiModal::handleClick(const GuiMouseEvent& event) {
  (void)GuiPanel::handleClick(event);
  if (dismiss == GuiModalDismiss::BACKDROP_OR_CANCEL && on_dismiss) {
    on_dismiss();
  }
  return true;
}

bool GuiModal::handleNav(GuiNavCommand command) {
  if (command != GuiNavCommand::CANCEL || dismiss == GuiModalDismiss::NEVER) {
    return false;
  }
  if (on_dismiss) {
    on_dismiss();
  }
  return true;
}

void GuiModal::open(GuiWidgetTree& tree) {
  visible = true;
  fadeIn(fade_seconds, GuiEasing::EASE_OUT);
  tree.markDirty(widget_id);
  tree.setFocusScope(widget_id);
}

void GuiModal::close(GuiWidgetTree& tree) {
  visible = false;
  if (tree.focus_scope_id == widget_id) {
    tree.setFocusScope(GUI_WIDGET_ID_INVALID);
  }
}

GuiWidgetId GuiModal::addCard(GuiWidgetTree& tree, float width) {
  auto card = std::make_unique<GuiCard>();
  card->debug_name = "gui-modal-card";
  card->tree_layout.width = width;
  card->tree_layout.padding = {CARD_PADDING, CARD_PADDING, CARD_PADDING,
                               CARD_PADDING};
  card->tree_layout.gap = CARD_GAP;
  // A dialog lifts off the page as high as anything does.
  card->elevation = GuiElevation::HIGH;
  return tree.insertExternalWidget(std::move(card), widget_id);
}

}  // namespace eng
