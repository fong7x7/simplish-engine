#pragma once

/// @file gui-modal.h
/// @brief A dialog over everything else: dims it, blocks it, holds focus.
/// @par Threading
/// Main thread only.

#include "gui-modal-dismiss.h"
#include "gui-panel.h"
#include "gui-widget-tree.h"

#include <functional>

namespace eng {

/// A modal: a see-through scrim over the whole of its parent — the tree's
/// overlay layer — that swallows the pointer, keeps navigation inside it,
/// and centres its content: HTML's `<dialog>` with its `::backdrop`.
///
/// ```cpp
/// const GuiWidgetId id = tree.insertExternalWidget(
///     std::make_unique<GuiModal>(), tree.overlayLayer());
/// auto& modal = *dynamic_cast<GuiModal*>(tree.findWidget(id));
/// const GuiWidgetId card = modal.addCard(tree, 360.0f);  // put content here
/// modal.on_dismiss = [&] { modal.close(tree); };
/// modal.open(tree);
/// ```
/// See `technical/overlays.md`.
/// @thread_safety Main thread only.
class GuiModal : public GuiPanel {
public:
  /// A hidden modal covering its parent, its content centred.
  GuiModal();

  /// Polymorphic deep-copy.
  [[nodiscard]] std::unique_ptr<GuiWidget> clone() const override;

  /// The theme's scrim over its whole rect.
  void render(const GuiDrawContext& ctx) const override;

  /// Swallow every press, so nothing under it is reached.
  bool handleMouseDown(const GuiMouseEvent& event) override;

  /// A click on the backdrop itself — not its content — dismisses it when
  /// `dismiss` allows.
  bool handleClick(const GuiMouseEvent& event) override;

  /// CANCEL dismisses it unless `dismiss` is NEVER.
  bool handleNav(GuiNavCommand command) override;

  /// Show it, keep navigation inside it, fade it in, and ask for a layout.
  void open(GuiWidgetTree& tree);

  /// Hide it and let navigation range over the tree again.
  void close(GuiWidgetTree& tree);

  /// Add the dialog's box: a raised card @p width wide in the theme's
  /// card look, padded and spacing its children. Returns its id; put the
  /// dialog's title, text and buttons under it.
  GuiWidgetId addCard(GuiWidgetTree& tree, float width);

  /// Called when it is dismissed — backdrop or CANCEL — not when one of
  /// its buttons closes it. Usually calls `close`.
  std::function<void()> on_dismiss{};
  /// What dismisses it.
  GuiModalDismiss dismiss = GuiModalDismiss::BACKDROP_OR_CANCEL;
  /// Seconds it takes to fade in.
  float fade_seconds = 0.15f;
};

}  // namespace eng
