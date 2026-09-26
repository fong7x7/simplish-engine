#include <catch2/catch_test_macros.hpp>
#include <engine/gui/gui-card.h>
#include <engine/gui/gui-modal.h>
#include <memory>

using namespace eng;

namespace {

/// A tree with a button under a modal in the overlay layer.
struct ModalFixture {
  GuiWidgetTree tree;
  GuiWidgetId root{
      tree.createWidget(GuiWidgetType::PANEL, GUI_WIDGET_ID_INVALID)};
  GuiWidgetId under{tree.createWidget(GuiWidgetType::BUTTON, root)};
  GuiModal* modal = nullptr;
  GuiWidgetId card = GUI_WIDGET_ID_INVALID;
  int dismissed = 0;
  int pressed_under = 0;

  ModalFixture() {
    tree.findWidget(under)->tree_layout.height = 40.0f;
    tree.findWidget(under)->onClick(
        [this](const GuiMouseEvent&) { ++pressed_under; });
    const GuiWidgetId id = tree.insertExternalWidget(
        std::make_unique<GuiModal>(), tree.overlayLayer());
    modal = dynamic_cast<GuiModal*>(tree.findWidget(id));
    card = modal->addCard(tree, 200.0f);
    tree.findWidget(card)->tree_layout.height = 100.0f;
    modal->on_dismiss = [this] {
      ++dismissed;
      modal->close(tree);
    };
    modal->open(tree);
    tree.computeLayout({0, 0, 400, 300});
  }
};

}  // namespace

TEST_CASE("a modal covers everything and centres its card") {
  ModalFixture fx;
  CHECK(fx.modal->rect.w == 400.0f);
  const Rect& card = fx.tree.findWidget(fx.card)->rect;
  CHECK(card.x == 100.0f);
  CHECK(card.y == 100.0f);
  CHECK(fx.tree.focus_scope_id == fx.modal->widget_id);
}

TEST_CASE("an open modal blocks the pointer from what is under it") {
  ModalFixture fx;
  fx.tree.dispatchMouseDown({.x = 20, .y = 20});
  fx.tree.dispatchMouseUp({.x = 20, .y = 20});
  CHECK(fx.pressed_under == 0);
  // The click landed on the backdrop, which dismisses: at once for the
  // pointer, and out of sight when its fade ends.
  CHECK(fx.dismissed == 1);
  CHECK(fx.modal->pointer_through);
  fx.tree.updateAll({}, 1.0f);
  CHECK_FALSE(fx.modal->visible);
}

TEST_CASE("a click on the card does not dismiss; CANCEL does unless NEVER") {
  ModalFixture fx;
  fx.tree.dispatchMouseDown({.x = 200, .y = 150});
  fx.tree.dispatchMouseUp({.x = 200, .y = 150});
  CHECK(fx.dismissed == 0);
  fx.modal->dismiss = GuiModalDismiss::NEVER;
  CHECK_FALSE(fx.modal->handleNav(GuiNavCommand::CANCEL));
  fx.modal->dismiss = GuiModalDismiss::CANCEL_ONLY;
  CHECK(fx.modal->handleNav(GuiNavCommand::CANCEL));
  CHECK(fx.dismissed == 1);
}

TEST_CASE("an opening modal's card pops up to rest") {
  ModalFixture fx;
  const GuiWidget& card = *fx.tree.findWidget(fx.card);
  CHECK(card.render_scale == GUI_PRESENCE_POP.scale);
  CHECK(card.opacity == 0.0f);
  fx.tree.updateAll({}, 1.0f);
  CHECK(card.render_scale == 1.0f);
  CHECK(card.opacity == 1.0f);
}

TEST_CASE("with reduced motion, a modal lands and leaves in one frame") {
  ModalFixture fx;
  GuiDrawContext still;
  still.motion = GuiMotion::REDUCED;
  fx.tree.updateAll(still, 0.001f);
  CHECK(fx.tree.findWidget(fx.card)->opacity == 1.0f);
  fx.modal->close(fx.tree);
  fx.tree.updateAll(still, 0.001f);
  CHECK_FALSE(fx.modal->visible);
}
