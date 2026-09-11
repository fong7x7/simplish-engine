#include <editor/shell/editor-character-select-widget.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <string_view>
#include <utility>

namespace eng::editor {

namespace {

  /// Laid over the level while the selector is open.
  constexpr GuiColor BACKDROP{0, 0, 0, 150};
  constexpr GuiColor CARD_FILL = THEME_BTN;
  /// A card's picture well, for a card with no picture or none yet.
  constexpr GuiColor PICTURE_WELL = THEME_BG;
  constexpr GuiColor PICTURE_TINT{255, 255, 255, 255};
  /// How far text sits in from a card's left edge.
  constexpr float TEXT_INSET = 8.0f;
  /// Height of one line of a card's text.
  constexpr float LINE_HEIGHT = 18.0f;
  constexpr std::string_view TITLE = "Choose your character";
  constexpr std::string_view HINT =
      "Click a character, or arrows and Enter  ·  Esc to cancel";

  /// The line @p line of text under a card's picture.
  Rect textLine(const Rect& card, int line) {
    const Rect picture = characterCardPictureRect(card);
    return makeRect(card.x + TEXT_INSET,
                    picture.y + picture.h + 4.0f +
                        static_cast<float>(line) * LINE_HEIGHT,
                    card.w - 2.0f * TEXT_INSET, LINE_HEIGHT);
  }

}  // namespace

EditorCharacterSelectWidget::EditorCharacterSelectWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-character-select";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  visible = false;
}

std::unique_ptr<GuiWidget> EditorCharacterSelectWidget::clone() const {
  return std::make_unique<EditorCharacterSelectWidget>(*this);
}

void EditorCharacterSelectWidget::open(std::vector<EditorCharacterCard> cards,
                                       size_t highlighted) {
  cards_ = std::move(cards);
  highlighted_ = highlighted < cards_.size() ? highlighted : 0;
  visible = isOpen();
}

void EditorCharacterSelectWidget::close() {
  cards_.clear();
  highlighted_ = 0;
  visible = false;
}

void EditorCharacterSelectWidget::moveHighlight(int steps) {
  if (cards_.empty()) {
    return;
  }
  const auto n = static_cast<long>(cards_.size());
  const long to = ((static_cast<long>(highlighted_) + steps) % n + n) % n;
  highlighted_ = static_cast<size_t>(to);
}

void EditorCharacterSelectWidget::confirm() const {
  if (isOpen() && on_chosen) {
    on_chosen(highlighted_);
  }
}

void EditorCharacterSelectWidget::cancel() const {
  if (isOpen() && on_cancelled) {
    on_cancelled();
  }
}

EditorCharacterSelectLayout EditorCharacterSelectWidget::layout() const {
  return layoutEditorCharacterSelect(rect, cards_.size());
}

void EditorCharacterSelectWidget::renderPicture(const GuiDrawContext& ctx,
                                                const Rect& card,
                                                size_t index) const {
  const Rect picture = characterCardPictureRect(card);
  ctx.drawFilledRect(picture, GuiColor::applyOpacity(PICTURE_WELL, opacity));
  if (cards_[index].picture != RHI_TEXTURE_INVALID) {
    ctx.drawTexturedRect({picture, cards_[index].picture,
                          GuiColor::applyOpacity(PICTURE_TINT, opacity)});
  }
}

void EditorCharacterSelectWidget::renderCard(const GuiDrawContext& ctx,
                                             const Rect& card,
                                             size_t index) const {
  const bool lit = index == highlighted_;
  ctx.drawRoundedRect(card, GuiColor::applyOpacity(CARD_FILL, opacity),
                      THEME_BTN_RADIUS);
  renderPicture(ctx, card, index);
  const EditorCharacterCard& shown = cards_[index];
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               drawPosInset(textLine(card, 0), 0.0f, 0.0f), shown.name);
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               drawPosInset(textLine(card, 1), 0.0f, 0.0f), shown.speed);
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               drawPosInset(textLine(card, 2), 0.0f, 0.0f), shown.health);
  if (lit) {
    ctx.drawBorderRect(card, GuiColor::applyOpacity(THEME_ACCENT, opacity));
  }
}

void EditorCharacterSelectWidget::render(const GuiDrawContext& ctx) const {
  if (!isOpen() || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  const EditorCharacterSelectLayout parts = layout();
  ctx.drawFilledRect(rect, GuiColor::applyOpacity(BACKDROP, opacity));
  ctx.drawRoundedRect(parts.panel, GuiColor::applyOpacity(fill_color, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(parts.title, GuiColor::applyOpacity(THEME_TEXT, opacity),
                       TITLE);
  ctx.drawCenteredText(parts.hint, GuiColor::applyOpacity(THEME_DIM, opacity),
                       HINT);
  for (size_t i = 0; i < cards_.size(); ++i) {
    renderCard(ctx, characterCardRect(parts, i), i);
  }
}

bool EditorCharacterSelectWidget::handleMouseDown(const GuiMouseEvent& event) {
  if (!isOpen() || event.button != GuiMouseButton::LEFT) {
    return false;
  }
  const EditorCharacterSelectLayout parts = layout();
  const int card = hitTestCharacterCard(parts, cards_.size(), event.x, event.y);
  if (card >= 0) {
    highlighted_ = static_cast<size_t>(card);
    confirm();
  } else if (!containsPoint(parts.panel, event.x, event.y)) {
    cancel();
  }
  return false;
}

}  // namespace eng::editor
