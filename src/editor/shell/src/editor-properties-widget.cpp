#include <algorithm>
#include <editor/shell/editor-emitter-ops.h>
#include <editor/shell/editor-entity-id.h>
#include <editor/shell/editor-light-ops.h>
#include <editor/shell/editor-player-start-ops.h>
#include <editor/shell/editor-properties-widget.h>
#include <editor/shell/editor-property-ops.h>
#include <editor/shell/editor-scale-slider.h>
#include <editor/shell/editor-sprite-ops.h>
#include <editor/shell/editor-waypoint-ops.h>
#include <engine/gui/gui-draw-context.h>
#include <engine/gui/gui-theme-constants.h>
#include <utility>

namespace eng::editor {

namespace {

  constexpr GuiColor VALUE_BG{30, 30, 34, 255};
  constexpr GuiColor STEP_BG{55, 55, 60, 255};
  /// Text baseline inset within a row, from its top.
  constexpr float TEXT_BASELINE = 4.0f;
  constexpr float TEXT_INSET = 6.0f;
  /// How far a checkbox's tick sits inside its box.
  constexpr float CHECK_INSET = 5.0f;
  /// The part of a slider's track already travelled, behind the readout.
  constexpr GuiColor SLIDER_FILL{0, 122, 204, 110};
  /// The mark at 1 on a scale slider, the size an asset was dropped at.
  constexpr GuiColor SLIDER_MARK{120, 120, 120, 255};
  /// Width of the slider's handle and of the mark at 1.
  constexpr float SLIDER_LINE = 2.0f;

  /// A full-height line across a slider's @p track at @p x: its handle, or
  /// the mark at 1.
  Rect sliderLineRect(const Rect& track, float x) {
    return makeRect(x - SLIDER_LINE * 0.5f, track.y, SLIDER_LINE, track.h);
  }

  /// The part of @p track from its left end to the handle at @p knob.
  Rect sliderTravelledRect(const Rect& track, float knob) {
    return makeRect(track.x, track.y, knob - track.x, track.h);
  }

  /// Vertically centred draw position for a line of text in @p rect.
  DrawPos textPos(const Rect& rect, float inset_x) {
    return drawPosInset(rect, inset_x, TEXT_BASELINE);
  }

  /// Distance from one row's top to the next row's.
  constexpr float ROW_PITCH = PROPERTIES_ROW_HEIGHT + PROPERTIES_ROW_GAP;
  /// Width of the scroll thumb down the rows' right edge.
  constexpr float SCROLL_THUMB_WIDTH = 3.0f;
  /// The thumb, over the panel's own colour.
  constexpr GuiColor SCROLL_THUMB{110, 110, 118, 200};

  /// The choice @p steps along from @p current among @p count, wrapping
  /// round at either end whichever way it stepped and however far.
  size_t stepChoice(size_t count, size_t current, int steps) {
    const auto n = static_cast<long>(count);
    const long to = ((static_cast<long>(current) + steps) % n + n) % n;
    return static_cast<size_t>(to);
  }

}  // namespace

EditorPropertiesWidget::EditorPropertiesWidget() {
  widget_type = GuiWidgetType::PANEL;
  debug_name = "editor-properties";
  fill_color = THEME_PANEL;
  border_color = THEME_BORDER;
  border_width = 1.0f;
  visible = false;
}

std::unique_ptr<GuiWidget> EditorPropertiesWidget::clone() const {
  return std::make_unique<EditorPropertiesWidget>(*this);
}

float EditorPropertiesWidget::preferredWidth() const {
  return has_selection_ ? PROPERTIES_PANEL_WIDTH : 0.0f;
}

EditorPropertiesLayout EditorPropertiesWidget::layout() const {
  return layoutEditorProperties(rect);
}

size_t EditorPropertiesWidget::rowOf(EditorPropertyField field) const {
  size_t row = 0;
  while (row < fields_.size() && fields_[row] != field) {
    ++row;
  }
  return row;
}

float EditorPropertiesWidget::value(EditorPropertyField field) const {
  const size_t row = rowOf(field);
  return row < values_.size() ? values_[row] : 0.0f;
}

Rect EditorPropertiesWidget::fieldRowRect(EditorPropertyField field) const {
  const size_t row = rowOf(field);
  // A field this selection does not list has no row to point at, and an
  // out-of-range one would be another field's.
  if (row >= fields_.size()) {
    return {};
  }
  return propertyRowRect(scrolledBody(), fieldSlot(row));
}

void EditorPropertiesWidget::beginSelection(
    std::string name, std::string reference,
    std::span<const EditorPropertyField> fields) {
  // Another selection starts at the top; the same one shown again — as it
  // is after every edit — keeps its place in a long list.
  if (reference != reference_) {
    scroll_ = 0.0f;
  }
  name_ = std::move(name);
  reference_ = std::move(reference);
  fields_.assign(fields.begin(), fields.end());
  values_.assign(fields_.size(), 0.0f);
  choice_rows_.clear();
  has_selection_ = true;
  visible = true;
}

void EditorPropertiesWidget::setGroundSelection(std::string name,
                                                std::string reference) {
  beginSelection(std::move(name), std::move(reference), {});
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorPlacement& placement) {
  beginSelection(std::move(name), editorPlacementRef(placement),
                 EDITOR_PLACEMENT_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorPropertyValue(placement, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorLight& light) {
  beginSelection(std::move(name), editorLightRef(light),
                 editorLightFields(light.kind));
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorLightValue(light, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorPlayerStart& start) {
  beginSelection(std::move(name), editorPlayerStartRef(start),
                 EDITOR_PLAYER_START_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorPlayerStartValue(start, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorWaypoint& waypoint) {
  beginSelection(std::move(name), editorWaypointRef(waypoint),
                 EDITOR_WAYPOINT_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorWaypointValue(waypoint, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorEmitter& emitter) {
  beginSelection(std::move(name), editorEmitterRef(emitter),
                 EDITOR_EMITTER_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorEmitterValue(emitter, fields_[row]);
  }
}

void EditorPropertiesWidget::setSelection(std::string name,
                                          const EditorSprite& sprite) {
  beginSelection(std::move(name), editorSpriteRef(sprite),
                 EDITOR_SPRITE_FIELDS);
  for (size_t row = 0; row < fields_.size(); ++row) {
    values_[row] = editorSpriteValue(sprite, fields_[row]);
  }
}

void EditorPropertiesWidget::addChoices(EditorChoiceKind kind,
                                        std::vector<std::string> choices,
                                        size_t current) {
  const size_t at = choiceRowOf(kind);
  if (choices.empty()) {
    if (at < choice_rows_.size()) {
      choice_rows_.erase(choice_rows_.begin() + static_cast<long>(at));
    }
    return;
  }
  const size_t shown = current < choices.size() ? current : 0;
  placeChoiceRow({kind, std::move(choices), shown});
}

void EditorPropertiesWidget::placeChoiceRow(EditorChoiceRow row) {
  const size_t at = choiceRowOf(row.kind);
  if (at < choice_rows_.size()) {
    choice_rows_[at] = std::move(row);
    return;
  }
  // A leading row goes after those that already lead and before the rest,
  // so the list stays leading rows first, which is what the slots count on.
  const size_t before =
      editorChoiceLeads(row.kind) ? leadingChoiceCount() : choice_rows_.size();
  choice_rows_.insert(choice_rows_.begin() + static_cast<long>(before),
                      std::move(row));
}

size_t EditorPropertiesWidget::choiceRowOf(EditorChoiceKind kind) const {
  size_t at = 0;
  while (at < choice_rows_.size() && choice_rows_[at].kind != kind) {
    ++at;
  }
  return at;
}

const std::string& EditorPropertiesWidget::choice(EditorChoiceKind kind) const {
  static const std::string none;
  const size_t at = choiceRowOf(kind);
  return at < choice_rows_.size()
             ? choice_rows_[at].names[choice_rows_[at].current]
             : none;
}

size_t EditorPropertiesWidget::choiceIndex(EditorChoiceKind kind) const {
  const size_t at = choiceRowOf(kind);
  return at < choice_rows_.size() ? choice_rows_[at].current : 0;
}

bool EditorPropertiesWidget::hasChoiceRow(EditorChoiceKind kind) const {
  return choiceRowOf(kind) < choice_rows_.size();
}

size_t EditorPropertiesWidget::rowCount() const {
  return fields_.size() + choice_rows_.size();
}

size_t EditorPropertiesWidget::leadingChoiceCount() const {
  return static_cast<size_t>(
      std::ranges::count_if(choice_rows_, [](const EditorChoiceRow& row) {
        return editorChoiceLeads(row.kind);
      }));
}

size_t EditorPropertiesWidget::fieldSlot(size_t row) const {
  return leadingChoiceCount() + row;
}

size_t EditorPropertiesWidget::choiceSlot(size_t index) const {
  return index < leadingChoiceCount() ? index : fields_.size() + index;
}

float EditorPropertiesWidget::maxScroll() const {
  const float content = static_cast<float>(rowCount()) * ROW_PITCH;
  return std::max(0.0f, content - PROPERTIES_ROW_GAP - layout().body.h);
}

float EditorPropertiesWidget::scrollOffset() const {
  return std::clamp(scroll_, 0.0f, maxScroll());
}

Rect EditorPropertiesWidget::scrolledBody() const {
  Rect body = layout().body;
  body.y -= scrollOffset();
  return body;
}

Rect EditorPropertiesWidget::choiceRowRectAt(size_t index) const {
  return propertyRowRect(scrolledBody(), choiceSlot(index));
}

Rect EditorPropertiesWidget::choiceRowRect(EditorChoiceKind kind) const {
  const size_t at = choiceRowOf(kind);
  return at < choice_rows_.size() ? choiceRowRectAt(at) : Rect{};
}

void EditorPropertiesWidget::clearSelection() {
  choice_rows_.clear();
  has_selection_ = false;
  visible = false;
  // A drag whose subject has gone has nothing left to commit.
  dragging_ = false;
  name_.clear();
  reference_.clear();
  fields_.clear();
  values_.clear();
}

void EditorPropertiesWidget::renderHeader(const GuiDrawContext& ctx) const {
  const Rect header = layout().header;
  ctx.drawFilledRect(header, GuiColor::applyOpacity(THEME_BG, opacity));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(header, TEXT_INSET), "Properties");
}

void EditorPropertiesWidget::renderNameLine(const GuiDrawContext& ctx) const {
  const Rect line = layout().asset;
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               textPos(line, TEXT_INSET), name_);
}

void EditorPropertiesWidget::renderIdLine(const GuiDrawContext& ctx) const {
  const Rect line = layout().id;
  // Dimmer than the name above it, and the panel's one piece of text that
  // is not prose: this is a reference to be copied verbatim.
  ctx.drawText(GuiColor::applyOpacity(THEME_DIM, opacity),
               textPos(line, TEXT_INSET), reference_);
}

void EditorPropertiesWidget::renderStep(const GuiDrawContext& ctx,
                                        const Rect& box,
                                        std::string_view sign) const {
  ctx.drawRoundedRect(box, GuiColor::applyOpacity(STEP_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(box, GuiColor::applyOpacity(THEME_TEXT, opacity), sign);
}

void EditorPropertiesWidget::renderToggle(const GuiDrawContext& ctx,
                                          const Rect& row, size_t index) const {
  const Rect box = propertyCheckboxRect(row);
  ctx.drawRoundedRect(box, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  if (values_[index] == 0.0f) {
    return;
  }
  // Ticked: the box filled in the accent colour, inset so the unticked
  // well still frames it.
  const Rect tick =
      makeRect(box.x + CHECK_INSET, box.y + CHECK_INSET,
               box.w - CHECK_INSET * 2.0f, box.h - CHECK_INSET * 2.0f);
  ctx.drawRoundedRect(tick, GuiColor::applyOpacity(THEME_ACCENT, opacity),
                      THEME_BTN_RADIUS);
}

void EditorPropertiesWidget::renderRow(const GuiDrawContext& ctx,
                                       size_t index) const {
  const EditorPropertyField field = fields_[index];
  const Rect row = propertyRowRect(scrolledBody(), fieldSlot(index));
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(propertyLabelRect(row), 0.0f),
               editorPropertyFieldLabel(field));
  if (editorPropertyFieldIsToggle(field)) {
    renderToggle(ctx, row, index);
    return;
  }
  renderStep(ctx, propertyDecrementRect(row), "-");
  renderStep(ctx, propertyIncrementRect(row), "+");
  if (editorPropertyFieldIsScale(field)) {
    renderSlider(ctx, propertyValueRect(row), index);
    return;
  }
  renderValueBox(ctx, row, index);
}

void EditorPropertiesWidget::renderSlider(const GuiDrawContext& ctx,
                                          const Rect& track,
                                          size_t index) const {
  const float knob =
      track.x + track.w * editorScaleSliderFraction(values_[index]);
  ctx.drawRoundedRect(track, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawRoundedRect(sliderTravelledRect(track, knob),
                      GuiColor::applyOpacity(SLIDER_FILL, opacity),
                      THEME_BTN_RADIUS);
  // The middle of the track is 1 exactly, so a mark there says where the
  // size a prop was dropped at is, whichever way it has been pushed.
  ctx.drawFilledRect(sliderLineRect(track, track.x + track.w * 0.5f),
                     GuiColor::applyOpacity(SLIDER_MARK, opacity));
  ctx.drawFilledRect(sliderLineRect(track, knob),
                     GuiColor::applyOpacity(THEME_TEXT, opacity));
  ctx.drawCenteredText(
      track, GuiColor::applyOpacity(THEME_TEXT, opacity),
      formatEditorPropertyValue(values_[index], fields_[index]));
}

void EditorPropertiesWidget::renderValueBox(const GuiDrawContext& ctx,
                                            const Rect& row,
                                            size_t index) const {
  const EditorPropertyField field = fields_[index];
  const Rect value = propertyValueRect(row);
  const bool active = dragging_ && drag_field_ == field;
  ctx.drawRoundedRect(
      value, GuiColor::applyOpacity(active ? THEME_ACCENT : VALUE_BG, opacity),
      THEME_BTN_RADIUS);
  ctx.drawCenteredText(value, GuiColor::applyOpacity(THEME_TEXT, opacity),
                       formatEditorPropertyValue(values_[index], field));
}

void EditorPropertiesWidget::renderRows(const GuiDrawContext& ctx) const {
  // Clipped to the area the rows are laid out in, so a row scrolled half
  // out of it does not draw over the id line or off the panel's foot.
  if (ctx.renderer != nullptr) {
    ctx.renderer->pushScissor(layout().body);
  }
  for (size_t row = 0; row < fields_.size(); ++row) {
    renderRow(ctx, row);
  }
  for (size_t row = 0; row < choice_rows_.size(); ++row) {
    renderChoiceRow(ctx, row);
  }
  if (ctx.renderer != nullptr) {
    ctx.renderer->popScissor();
  }
}

void EditorPropertiesWidget::renderScrollbar(const GuiDrawContext& ctx) const {
  const float most = maxScroll();
  if (most <= 0.0f) {
    return;
  }
  const Rect body = layout().body;
  const float shown = body.h / (body.h + most);
  const float thumb = body.h * shown;
  const float y = body.y + (body.h - thumb) * (scrollOffset() / most);
  ctx.drawFilledRect(makeRect(body.x + body.w - SCROLL_THUMB_WIDTH, y,
                              SCROLL_THUMB_WIDTH, thumb),
                     GuiColor::applyOpacity(SCROLL_THUMB, opacity));
}

void EditorPropertiesWidget::renderChoiceRow(const GuiDrawContext& ctx,
                                             size_t index) const {
  const EditorChoiceRow& choices = choice_rows_[index];
  const Rect row = choiceRowRectAt(index);
  ctx.drawText(GuiColor::applyOpacity(THEME_TEXT, opacity),
               textPos(propertyLabelRect(row), 0.0f),
               editorChoiceLabel(choices.kind));
  renderStep(ctx, propertyDecrementRect(row), "-");
  renderStep(ctx, propertyIncrementRect(row), "+");
  const Rect value = propertyValueRect(row);
  ctx.drawRoundedRect(value, GuiColor::applyOpacity(VALUE_BG, opacity),
                      THEME_BTN_RADIUS);
  ctx.drawCenteredText(value, GuiColor::applyOpacity(THEME_TEXT, opacity),
                       choices.names[choices.current]);
}

void EditorPropertiesWidget::render(const GuiDrawContext& ctx) const {
  if (!has_selection_ || rect.w <= 0.0f || rect.h <= 0.0f) {
    return;
  }
  renderPanel({ctx, GuiColor::applyOpacity(fill_color, opacity)});
  renderHeader(ctx);
  renderNameLine(ctx);
  renderIdLine(ctx);
  renderRows(ctx);
  renderScrollbar(ctx);
}

void EditorPropertiesWidget::applyValue(EditorPropertyField field, float value,
                                        EditorPropertyEdit edit) {
  const size_t row = rowOf(field);
  if (row >= values_.size()) {
    return;
  }
  // Normalised here, through the same rule the document applies, so what
  // the panel draws next frame is what the document will hold — a wrapped
  // angle and a clamped colour included.
  values_[row] = normalizeEditorPropertyValue(field, value);
  if (on_property_changed) {
    on_property_changed(field, values_[row], edit);
  }
}

void EditorPropertiesWidget::stepField(EditorPropertyField field, float steps) {
  // A scale steps between fixed stops, not by an amount: see
  // `editorScaleStepped` for why that is what lands it back on 1.
  const float stepped =
      editorPropertyFieldIsScale(field)
          ? editorScaleStepped(value(field), static_cast<int>(steps))
          : value(field) + steps * editorPropertyStep(field);
  applyValue(field, stepped, EditorPropertyEdit::COMMIT);
}

float EditorPropertiesWidget::sliderValueAt(EditorPropertyField field,
                                            float x) const {
  const Rect track = propertyValueRect(fieldRowRect(field));
  const float fraction = track.w > 0.0f ? (x - track.x) / track.w : 0.5f;
  return editorScaleFromSliderFraction(fraction);
}

bool EditorPropertiesWidget::pressStep(EditorPropertyField field,
                                       const Rect& row,
                                       const GuiMouseEvent& event) {
  if (containsPoint(propertyDecrementRect(row), event.x, event.y)) {
    stepField(field, -1.0f);
    return true;
  }
  if (containsPoint(propertyIncrementRect(row), event.x, event.y)) {
    stepField(field, 1.0f);
    return true;
  }
  return false;
}

void EditorPropertiesWidget::beginDrag(EditorPropertyField field,
                                       const GuiMouseEvent& event) {
  dragging_ = true;
  drag_field_ = field;
  drag_start_value_ = value(field);
  drag_start_x_ = event.x;
  if (editorPropertyFieldIsScale(field)) {
    // Straight to where it was pressed, as a slider does, rather than
    // waiting for the pointer to move before anything happens.
    applyValue(field, sliderValueAt(field, event.x),
               EditorPropertyEdit::PREVIEW);
  }
}

bool EditorPropertiesWidget::pressRow(size_t index,
                                      const GuiMouseEvent& event) {
  const EditorPropertyField field = fields_[index];
  // A toggle flips on a press anywhere in its row, label included, and is
  // done at once: there is no gesture to capture.
  if (editorPropertyFieldIsToggle(field)) {
    applyValue(field, values_[index] == 0.0f ? 1.0f : 0.0f,
               EditorPropertyEdit::COMMIT);
    return false;
  }
  const Rect row = propertyRowRect(scrolledBody(), fieldSlot(index));
  // A step is done the moment it is pressed, so it never takes capture.
  if (pressStep(field, row, event) ||
      !containsPoint(propertyValueRect(row), event.x, event.y)) {
    return false;
  }
  beginDrag(field, event);
  return true;
}

void EditorPropertiesWidget::pressChoiceRow(size_t index,
                                            const GuiMouseEvent& event) {
  EditorChoiceRow& choices = choice_rows_[index];
  const Rect row = choiceRowRectAt(index);
  int steps = 0;
  if (containsPoint(propertyDecrementRect(row), event.x, event.y)) {
    steps = -1;
  } else if (containsPoint(propertyIncrementRect(row), event.x, event.y)) {
    steps = 1;
  }
  const size_t next = stepChoice(choices.names.size(), choices.current, steps);
  if (steps == 0 || next == choices.current) {
    return;
  }
  choices.current = next;
  if (on_choice_changed) {
    on_choice_changed(choices.kind, next);
  }
}

bool EditorPropertiesWidget::pressSlot(size_t slot,
                                       const GuiMouseEvent& event) {
  const size_t leading = leadingChoiceCount();
  if (slot >= leading && slot < leading + fields_.size()) {
    return pressRow(slot - leading, event);
  }
  // A choice row: a step is done the moment it is pressed, so it never
  // takes capture.
  pressChoiceRow(slot < leading ? slot : slot - fields_.size(), event);
  return false;
}

bool EditorPropertiesWidget::handleMouseDown(const GuiMouseEvent& event) {
  // A row scrolled out of the area the rows are shown in is out of reach.
  if (!has_selection_ || event.button != GuiMouseButton::LEFT ||
      !containsPoint(layout().body, event.x, event.y)) {
    return false;
  }
  const int slot =
      hitTestPropertyRow(scrolledBody(), rowCount(), event.x, event.y);
  return slot >= 0 && pressSlot(static_cast<size_t>(slot), event);
}

bool EditorPropertiesWidget::handleScroll(const GuiScrollEvent& event) {
  if (!has_selection_ || maxScroll() <= 0.0f ||
      !containsPoint(layout().body, event.x, event.y)) {
    return false;
  }
  // Positive delta_y is away from the user, which walks the rows back up;
  // a notch of the wheel is a row.
  scroll_ =
      std::clamp(scrollOffset() - event.delta_y * ROW_PITCH, 0.0f, maxScroll());
  return true;
}

void EditorPropertiesWidget::handleMouseMove(const GuiMouseEvent& event) {
  if (!dragging_) {
    return;
  }
  // A slider's value is wherever the pointer is along it; a value box's is
  // how far the pointer has travelled since the press.
  const float next =
      editorPropertyFieldIsScale(drag_field_)
          ? sliderValueAt(drag_field_, event.x)
          : drag_start_value_ + (event.x - drag_start_x_) *
                                    editorPropertyDragPerPixel(drag_field_);
  applyValue(drag_field_, next, EditorPropertyEdit::PREVIEW);
}

void EditorPropertiesWidget::handleMouseUp(const GuiMouseEvent& /*event*/) {
  if (!dragging_) {
    return;
  }
  dragging_ = false;
  // The value is already where the last move put it; this says the gesture
  // is over, which is what turns the run of previews into one undo entry.
  applyValue(drag_field_, value(drag_field_), EditorPropertyEdit::COMMIT);
}

}  // namespace eng::editor
