#include <algorithm>
#include <engine/gui/gui-button.h>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-text-area.h>
#include <engine/gui/gui-text-input.h>
#include <engine/gui/gui-widget-tree.h>
#include <memory>
#include <ranges>
#include <vector>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#include <SDL3/SDL.h>
#pragma clang diagnostic pop

namespace eng {

namespace {

  std::unique_ptr<GuiWidget> createWidgetForType(GuiWidgetType type) {
    switch (type) {
      case GuiWidgetType::BUTTON:
        return std::make_unique<GuiButton>();
      case GuiWidgetType::TEXT:
        return std::make_unique<GuiLabel>();
      case GuiWidgetType::TEXT_INPUT:
        return std::make_unique<GuiTextInput>();
      case GuiWidgetType::TEXT_AREA:
        return std::make_unique<GuiTextArea>();
      default:
        return std::make_unique<GuiPanel>();
    }
  }

  constexpr float DOUBLE_CLICK_TIME = 0.4f;
  constexpr float DOUBLE_CLICK_RADIUS = 5.0f;

}  // namespace

std::unique_ptr<GuiWidget> GuiWidgetTree::makeTreeNode(GuiWidgetType type) {
  return createWidgetForType(type);
}

bool GuiWidgetTree::isAncestor(const GuiWidgetTree& tree, GuiWidgetId id,
                               GuiWidgetId ancestor_id) {
  auto current = id;
  while (current != GUI_WIDGET_ID_INVALID) {
    if (current == ancestor_id) {
      return true;
    }
    auto it = tree.widget_nodes.find(current);
    if (it == tree.widget_nodes.end()) {
      return false;
    }
    current = it->second->parent_id;
  }
  return false;
}

void GuiWidgetTree::removeFromParent(GuiWidgetTree& tree, GuiWidgetId child_id,
                                     GuiWidgetId parent_id) {
  auto pit = tree.widget_nodes.find(parent_id);
  if (pit == tree.widget_nodes.end()) {
    return;
  }
  auto& ch = pit->second->children;
  std::erase(ch, child_id);
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::collectDescendants(const GuiWidgetTree& tree,
                                       GuiWidgetId id,
                                       std::vector<GuiWidgetId>& out) {
  auto it = tree.widget_nodes.find(id);
  if (it == tree.widget_nodes.end()) {
    return;
  }
  for (auto child : it->second->children) {
    out.push_back(child);
    collectDescendants(tree, child, out);
  }
}

void GuiWidgetTree::clearReferences(GuiWidgetTree& tree, GuiWidgetId id) {
  if (tree.focused_id == id) {
    tree.focused_id = GUI_WIDGET_ID_INVALID;
  }
  if (tree.hovered_id == id) {
    tree.hovered_id = GUI_WIDGET_ID_INVALID;
  }
  if (tree.pressed_id == id) {
    tree.pressed_id = GUI_WIDGET_ID_INVALID;
  }
}

GuiWidgetId GuiWidgetTree::insertWidget(GuiWidgetTree& tree, GuiWidgetType type,
                                        GuiWidgetId parent) {
  auto id = tree.next_id++;
  auto node = makeTreeNode(type);
  node->widget_id = id;
  node->widget_type = type;
  node->parent_id = parent;
  tree.widget_nodes[id] = std::move(node);
  return id;
}

void GuiWidgetTree::attachWidget(GuiWidgetTree& tree, GuiWidgetId id,
                                 GuiWidgetId parent) {
  if (parent == GUI_WIDGET_ID_INVALID) {
    tree.root_id = id;
    return;
  }
  auto* p = tree.findWidget(parent);
  if (p != nullptr) {
    p->children.push_back(id);
  }
}

void GuiWidgetTree::detachFromTree(GuiWidgetTree& tree, GuiWidgetId id,
                                   GuiWidgetId parent_id) {
  if (parent_id != GUI_WIDGET_ID_INVALID) {
    removeFromParent(tree, id, parent_id);
  }
  if (tree.root_id == id) {
    tree.root_id = GUI_WIDGET_ID_INVALID;
  }
}

void GuiWidgetTree::eraseWidgets(GuiWidgetTree& tree,
                                 const std::vector<GuiWidgetId>& ids) {
  for (auto desc : ids) {
    clearReferences(tree, desc);
    tree.widget_nodes.erase(desc);
  }
}

GuiWidgetId GuiWidgetTree::cloneNodeInto(GuiWidgetTree& target,
                                         const GuiWidget& src,
                                         GuiWidgetId target_parent) {
  auto cloned = src.clone();
  auto new_id = target.next_id++;
  cloned->widget_id = new_id;
  cloned->parent_id = target_parent;
  cloned->children.clear();
  cloned->resetTransientState();
  target.widget_nodes[new_id] = std::move(cloned);
  return new_id;
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
GuiWidgetId GuiWidgetTree::adoptSubtreeRec(GuiWidgetTree& target,
                                           const GuiWidgetTree& source,
                                           GuiWidgetId source_id,
                                           GuiWidgetId target_parent) {
  const auto* src = source.findWidget(source_id);
  if (src == nullptr) {
    return GUI_WIDGET_ID_INVALID;
  }
  auto new_id = cloneNodeInto(target, *src, target_parent);
  for (auto child_id : src->children) {
    auto new_child = adoptSubtreeRec(target, source, child_id, new_id);
    if (new_child != GUI_WIDGET_ID_INVALID) {
      target.widget_nodes[new_id]->children.push_back(new_child);
    }
  }
  return new_id;
}

GuiWidgetId GuiWidgetTree::adoptSubtree(const GuiWidgetTree& source,
                                        GuiWidgetId source_root,
                                        GuiWidgetId target_parent) {
  if (source.findWidget(source_root) == nullptr) {
    return GUI_WIDGET_ID_INVALID;
  }
  if (target_parent != GUI_WIDGET_ID_INVALID &&
      !widget_nodes.contains(target_parent)) {
    return GUI_WIDGET_ID_INVALID;
  }
  auto new_id = adoptSubtreeRec(*this, source, source_root, target_parent);
  attachWidget(*this, new_id, target_parent);
  return new_id;
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
GuiWidgetId GuiWidgetTree::adoptSubtreeMapRec(const AdoptMapContext& ctx,
                                              GuiWidgetId source_id,
                                              GuiWidgetId target_parent) {
  const auto* src = ctx.source.findWidget(source_id);
  if (src == nullptr) {
    return GUI_WIDGET_ID_INVALID;
  }
  auto new_id = cloneNodeInto(ctx.target, *src, target_parent);
  ctx.id_map[source_id] = new_id;
  for (auto child_id : src->children) {
    auto nc = adoptSubtreeMapRec(ctx, child_id, new_id);
    if (nc != GUI_WIDGET_ID_INVALID) {
      ctx.target.widget_nodes[new_id]->children.push_back(nc);
    }
  }
  return new_id;
}

AdoptResult GuiWidgetTree::adoptSubtreeWithMap(const GuiWidgetTree& source,
                                               GuiWidgetId source_root,
                                               GuiWidgetId target_parent) {
  AdoptResult result;
  if (source.findWidget(source_root) == nullptr) {
    return result;
  }
  if (target_parent != GUI_WIDGET_ID_INVALID &&
      !widget_nodes.contains(target_parent)) {
    return result;
  }
  AdoptMapContext ctx{*this, source, result.id_map};
  result.new_root = adoptSubtreeMapRec(ctx, source_root, target_parent);
  attachWidget(*this, result.new_root, target_parent);
  return result;
}

GuiWidgetId
GuiWidgetTree::insertExternalWidget(std::unique_ptr<GuiWidget> widget,
                                    GuiWidgetId parent) {
  if (widget == nullptr) {
    return GUI_WIDGET_ID_INVALID;
  }
  if (parent != GUI_WIDGET_ID_INVALID && !widget_nodes.contains(parent)) {
    return GUI_WIDGET_ID_INVALID;
  }
  auto id = next_id++;
  widget->widget_id = id;
  widget->parent_id = parent;
  widget_nodes[id] = std::move(widget);
  attachWidget(*this, id, parent);
  return id;
}

GuiWidgetId GuiWidgetTree::createWidget(GuiWidgetType type,
                                        GuiWidgetId parent) {
  if (parent != GUI_WIDGET_ID_INVALID && !widget_nodes.contains(parent)) {
    return GUI_WIDGET_ID_INVALID;
  }
  auto id = insertWidget(*this, type, parent);
  attachWidget(*this, id, parent);
  return id;
}

void GuiWidgetTree::destroyWidget(GuiWidgetId id) {
  auto it = widget_nodes.find(id);
  if (it == widget_nodes.end()) {
    return;
  }

  std::vector<GuiWidgetId> descendants;
  collectDescendants(*this, id, descendants);
  detachFromTree(*this, id, it->second->parent_id);
  clearReferences(*this, id);
  widget_nodes.erase(id);
  eraseWidgets(*this, descendants);
}

GuiWidget* GuiWidgetTree::findWidget(GuiWidgetId id) {
  auto it = widget_nodes.find(id);
  return (it != widget_nodes.end()) ? it->second.get() : nullptr;
}

const GuiWidget* GuiWidgetTree::findWidget(GuiWidgetId id) const {
  auto it = widget_nodes.find(id);
  return (it != widget_nodes.end()) ? it->second.get() : nullptr;
}

bool GuiWidgetTree::reparentWidget(GuiWidgetId id, GuiWidgetId new_parent) {
  if (isAncestor(*this, new_parent, id)) {
    return false;
  }
  auto* w = findWidget(id);
  if (w == nullptr || findWidget(new_parent) == nullptr) {
    return false;
  }

  if (w->parent_id != GUI_WIDGET_ID_INVALID) {
    removeFromParent(*this, id, w->parent_id);
  }
  w->parent_id = new_parent;
  findWidget(new_parent)->children.push_back(id);
  return true;
}

void GuiWidgetTree::markDirty(GuiWidgetId id) {
  auto current = id;
  while (current != GUI_WIDGET_ID_INVALID) {
    auto* w = findWidget(current);
    if (w == nullptr) {
      return;
    }
    w->tree_dirty = true;
    current = w->parent_id;
  }
}

void GuiWidgetTree::clearDirtyFlags() {
  for (auto& [id, node] : widget_nodes) {
    static_cast<void>(id);
    node->tree_dirty = false;
    node->tree_render_dirty = false;
  }
}

int32_t GuiWidgetTree::nodeZIndex(const GuiWidgetTree& tree, GuiWidgetId id) {
  const GuiWidget* w = tree.findWidget(id);
  return (w != nullptr) ? w->z_index : 0;
}

std::vector<GuiWidgetId>
GuiWidgetTree::sortedChildIdsByZ(const GuiWidgetTree& tree,
                                 const GuiWidget& parent) {
  std::vector<GuiWidgetId> ids = parent.children;
  std::ranges::stable_sort(ids, [&](GuiWidgetId a, GuiWidgetId b) {
    return nodeZIndex(tree, a) < nodeZIndex(tree, b);
  });
  return ids;
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::visitDrawOrderRec(const GuiWidgetTree& tree, GuiWidgetId id,
                                      const GuiWidgetVisitor& visitor) {
  const GuiWidget* w = tree.findWidget(id);
  if (w == nullptr || !w->visible) {
    return;
  }
  if (!w->overlay_registered) {
    visitor(*w);
  }
  for (GuiWidgetId cid : sortedChildIdsByZ(tree, *w)) {
    visitDrawOrderRec(tree, cid, visitor);
  }
}

std::vector<GuiWidget*>
GuiWidgetTree::sortedOverlaysZAsc(const std::vector<GuiWidget*>& comps) {
  auto out = comps;
  // NOLINTNEXTLINE(bugprone-nondeterministic-pointer-iteration-order) --
  // sorted by z_index, not pointer value
  std::ranges::stable_sort(out, [](const GuiWidget* a, const GuiWidget* b) {
    return a->z_index < b->z_index;
  });
  return out;
}

GuiWidget*
GuiWidgetTree::topHitInZAscOrder(const std::vector<GuiWidget*>& ord_asc,
                                 float x, float y) {
  for (auto* it : std::views::reverse(ord_asc)) {
    if (it->isInside(x, y)) {
      return it;
    }
  }
  return nullptr;
}

GuiTextInput*
GuiWidgetTree::topTextInputInZAscOrder(const std::vector<GuiWidget*>& ord_asc,
                                       float mx, float my) {
  for (auto* ri : std::views::reverse(ord_asc)) {
    auto* input = dynamic_cast<GuiTextInput*>(ri);
    if (input != nullptr && input->isInside(mx, my)) {
      return input;
    }
  }
  return nullptr;
}

GuiWidget* GuiWidgetTree::findTopOverlayAt(const std::vector<GuiWidget*>& comps,
                                           float x, float y) {
  return topHitInZAscOrder(sortedOverlaysZAsc(comps), x, y);
}

GuiWidget* GuiWidgetTree::resolveTreeHit(GuiWidgetTree& tree, float x,
                                         float y) {
  auto hit = tree.hitTest(x, y);
  if (hit.widget_id == GUI_WIDGET_ID_INVALID) {
    return nullptr;
  }
  return tree.findWidget(hit.widget_id);
}

GuiTextInput* GuiWidgetTree::asTextInputAt(GuiWidget* w, float mx, float my) {
  if (w == nullptr || !w->isInside(mx, my)) {
    return nullptr;
  }
  return dynamic_cast<GuiTextInput*>(w);
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
GuiTextInput* GuiWidgetTree::findTreeTextInput(GuiWidgetTree& tree,
                                               GuiWidgetId id, float mx,
                                               float my) {
  auto* w = tree.findWidget(id);
  if (w == nullptr || !w->visible) {
    return nullptr;
  }
  // Check children in reverse z-order (topmost first).
  auto sorted = sortedChildIdsByZ(tree, *w);
  for (GuiWidgetId it : std::views::reverse(sorted)) {
    auto* found = findTreeTextInput(tree, it, mx, my);
    if (found != nullptr) {
      return found;
    }
  }
  return asTextInputAt(w, mx, my);
}

void GuiWidgetTree::renderSortedOverlays(const std::vector<GuiWidget*>& comps,
                                         const GuiDrawContext& ctx) {
  for (GuiWidget* w : sortedOverlaysZAsc(comps)) {
    if (w->visible) {
      w->render(ctx);
    }
  }
}

void GuiWidgetTree::visitDrawOrder(const GuiWidgetVisitor& visitor) const {
  if (root_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  visitDrawOrderRec(*this, root_id, visitor);
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::visitPreOrder(GuiWidgetId root,
                                  const GuiWidgetVisitor& visitor) const {
  auto it = widget_nodes.find(root);
  if (it == widget_nodes.end()) {
    return;
  }
  visitor(*it->second);
  for (auto child : it->second->children) {
    visitPreOrder(child, visitor);
  }
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::visitPostOrder(GuiWidgetId root,
                                   const GuiWidgetVisitor& visitor) const {
  auto it = widget_nodes.find(root);
  if (it == widget_nodes.end()) {
    return;
  }
  for (auto child : it->second->children) {
    visitPostOrder(child, visitor);
  }
  visitor(*it->second);
}

// NOLINTNEXTLINE(misc-no-recursion) -- tree traversal requires recursion
void GuiWidgetTree::visitPreOrder(GuiWidgetId root,
                                  const GuiWidgetMutVisitor& visitor) {
  auto it = widget_nodes.find(root);
  if (it == widget_nodes.end()) {
    return;
  }
  visitor(*it->second);
  for (auto child : it->second->children) {
    visitPreOrder(child, visitor);
  }
}

size_t GuiWidgetTree::childCount(GuiWidgetId id) const {
  auto it = widget_nodes.find(id);
  return (it != widget_nodes.end()) ? it->second->children.size() : 0;
}

void GuiWidgetTree::setStyle(const GuiStyle& style) {
  active_style_ = &style;
  for (auto* comp : components_) {
    comp->ui_style = active_style_;
  }
}

void GuiWidgetTree::registerComponent(GuiWidget& comp) {
  if (active_style_ != nullptr) {
    comp.ui_style = active_style_;
  }
  comp.overlay_registered = true;
  components_.push_back(&comp);
}

void GuiWidgetTree::unregisterComponent(GuiWidget& comp) {
  if (focused_input_ == &comp) {
    focused_input_ = nullptr;
  }
  if (pending_click_target_ == &comp) {
    pending_click_target_ = nullptr;
  }
  comp.overlay_registered = false;
  auto it = std::ranges::find(components_, &comp);
  if (it != components_.end()) {
    if (captured_ == &comp) {
      captured_ = nullptr;
    }
    components_.erase(it);
  }
}

void GuiWidgetTree::clearComponents() {
  focused_input_ = nullptr;
  captured_ = nullptr;
  pending_click_target_ = nullptr;
  components_.clear();
}

void GuiWidgetTree::clearTreeHover() {
  for (auto& [id, node] : widget_nodes) {
    static_cast<void>(id);
    node->hovered = false;
  }
}

void GuiWidgetTree::applyTreeHover(GuiWidgetId leaf_id) {
  auto wid = leaf_id;
  while (wid != GUI_WIDGET_ID_INVALID) {
    auto* w = findWidget(wid);
    if (w == nullptr) {
      break;
    }
    w->hovered = true;
    wid = w->parent_id;
  }
}

void GuiWidgetTree::updateHover(float mx, float my) {
  auto* top_overlay = findTopOverlayAt(components_, mx, my);
  for (auto* comp : components_) {
    comp->hovered = (comp == top_overlay);
  }
  clearTreeHover();
  if (top_overlay != nullptr) {
    GuiMouseEvent evt{.x = mx, .y = my};
    top_overlay->handleMouseMove(evt);
  } else {
    auto hit = hitTest(mx, my);
    applyTreeHover(hit.widget_id);
    routeHoverMove(mx, my);
  }
  updateCursorForHover();
}

void GuiWidgetTree::routeHoverMove(float mx, float my) {
  auto* hit = resolveTreeHit(*this, mx, my);
  if (hit == nullptr) {
    return;
  }
  GuiMouseEvent evt{.x = mx, .y = my};
  hit->handleMouseMove(evt);
}

void GuiWidgetTree::accumulateCursorFlags(HoveredCursorFlags& flags,
                                          const GuiWidget& w) {
  if (!w.hovered) {
    return;
  }
  if (w.widget_type == GuiWidgetType::TEXT_INPUT ||
      w.widget_type == GuiWidgetType::TEXT_AREA) {
    flags.text_input = true;
  }
  if (w.widget_type == GuiWidgetType::BUTTON || w.hasClickHandlers()) {
    flags.clickable = true;
  }
}

GuiWidgetTree::HoveredCursorFlags
GuiWidgetTree::scanHoveredCursorFlags() const {
  HoveredCursorFlags flags{};
  for (const auto* c : components_) {
    accumulateCursorFlags(flags, *c);
  }
  for (const auto& [id, node] : widget_nodes) {
    static_cast<void>(id);
    accumulateCursorFlags(flags, *node);
  }
  return flags;
}

GuiCursorShape GuiWidgetTree::cursorShapeForHover() const {
  auto flags = scanHoveredCursorFlags();
  if (flags.text_input) {
    return GuiCursorShape::IBEAM;
  }
  return flags.clickable ? GuiCursorShape::POINTER : GuiCursorShape::ARROW;
}

void GuiWidgetTree::updateCursorForHover() {
  setUiCursor(cursorShapeForHover());
}

void GuiWidgetTree::applyTextInputFocusFromSortedOverlays(
    const std::vector<GuiWidget*>& ord_asc, float mx, float my) {
  applyTextInputFocus(topTextInputInZAscOrder(ord_asc, mx, my));
}

void GuiWidgetTree::applyTextInputFocus(GuiTextInput* hit) {
  if (hit == focused_input_) {
    return;
  }
  if (focused_input_ != nullptr) {
    focused_input_->focus = GuiTextInputFocus::UNFOCUSED;
  }
  focused_input_ = hit;
  if (focused_input_ != nullptr) {
    focused_input_->focus = GuiTextInputFocus::FOCUSED;
  }
}

void GuiWidgetTree::updateTextInputFocus(float mx, float my,
                                         GuiSelectionExtend sel_mode) {
  applyTextInputFocusFromSortedOverlays(sortedOverlaysZAsc(components_), mx,
                                        my);
  if (focused_input_ == nullptr && root_id != GUI_WIDGET_ID_INVALID) {
    applyTextInputFocus(findTreeTextInput(*this, root_id, mx, my));
  }
  if (focused_input_ != nullptr) {
    focused_input_->setCursorClickX(mx, sel_mode);
  }
}

void GuiWidgetTree::updateAll(const GuiDrawContext& ctx, float dt) {
  elapsed_time_ += dt;
  for (auto* comp : components_) {
    comp->update(ctx, dt);
  }
  for (auto& [id, node] : widget_nodes) {
    static_cast<void>(id);
    if (!node->overlay_registered) {
      node->update(ctx, dt);
    }
  }
}

bool GuiWidgetTree::dispatchClick(float mx, float my) {
  updateTextInputFocus(mx, my, GuiSelectionExtend::COLLAPSE);
  GuiMouseEvent event{.x = mx, .y = my};
  auto* comp = findTopOverlayAt(components_, mx, my);
  if (comp != nullptr) {
    comp->handleClick(event);
    return true;
  }
  auto* tree_hit = resolveTreeHit(*this, mx, my);
  if (tree_hit != nullptr) {
    tree_hit->handleClick(event);
    return true;
  }
  return focused_input_ != nullptr;
}

void GuiWidgetTree::renderAll(const GuiDrawContext& ctx) {
  visitDrawOrder([&](const GuiWidget& w) { w.render(ctx); });
  renderSortedOverlays(components_, ctx);
}

bool GuiWidgetTree::anyHovered() const {
  if (std::ranges::any_of(components_,
                          [](const GuiWidget* c) { return c->hovered; })) {
    return true;
  }
  return std::ranges::any_of(
      widget_nodes, [](const auto& pair) { return pair.second->hovered; });
}

GuiWidget* GuiWidgetTree::findById(std::string_view component_id) {
  auto it = std::ranges::find_if(
      components_, [&](const GuiWidget* c) { return c->id == component_id; });
  if (it != components_.end()) {
    return *it;
  }
  for (auto& [wid, node] : widget_nodes) {
    static_cast<void>(wid);
    if (node->id == component_id) {
      return node.get();
    }
  }
  return nullptr;
}

const GuiWidget* GuiWidgetTree::findById(std::string_view component_id) const {
  auto it = std::ranges::find_if(
      components_, [&](const GuiWidget* c) { return c->id == component_id; });
  if (it != components_.end()) {
    return *it;
  }
  for (const auto& [wid, node] : widget_nodes) {
    static_cast<void>(wid);
    if (node->id == component_id) {
      return node.get();
    }
  }
  return nullptr;
}

void GuiWidgetTree::handleClickWithDoubleCheck(const GuiMouseEvent& event,
                                               GuiWidget& target,
                                               GuiSelectionExtend drag_mode) {
  bool is_dbl = checkDoubleClick(event.x, event.y);
  recordClickTiming(event.x, event.y);
  auto sel_mode = event.shift_held ? GuiSelectionExtend::EXTEND : drag_mode;
  updateTextInputFocus(event.x, event.y, sel_mode);
  if (is_dbl && focused_input_ != nullptr) {
    focused_input_->setPendingWordSelect(event.x);
  }
  target.handleClick(event);
}

void GuiWidgetTree::tryFireClickFromMouseUp(const GuiMouseEvent& event,
                                            GuiSelectionExtend drag_mode) {
  if (pending_click_target_ == nullptr) {
    return;
  }
  if (event.button != pending_click_button_) {
    return;
  }
  GuiWidget* target = pending_click_target_;
  pending_click_target_ = nullptr;
  if (!target->isInside(event.x, event.y)) {
    return;
  }
  handleClickWithDoubleCheck(event, *target, drag_mode);
}

void GuiWidgetTree::beginTextInputDrag(GuiWidget* hit, float mx) {
  if (hit == nullptr) {
    return;
  }
  drag_input_ = dynamic_cast<GuiTextInput*>(hit);
  if (drag_input_ != nullptr) {
    drag_input_->setCursorClickX(mx, GuiSelectionExtend::COLLAPSE);
  }
}

bool GuiWidgetTree::checkDoubleClick(float mx, float my) const {
  float dt = elapsed_time_ - last_click_time_;
  float dx = mx - last_click_x_;
  float dy = my - last_click_y_;
  return dt < DOUBLE_CLICK_TIME &&
         (dx * dx + dy * dy) < DOUBLE_CLICK_RADIUS * DOUBLE_CLICK_RADIUS;
}

void GuiWidgetTree::recordClickTiming(float mx, float my) {
  last_click_time_ = elapsed_time_;
  last_click_x_ = mx;
  last_click_y_ = my;
}

GuiWidget* GuiWidgetTree::hitTestAny(float mx, float my) {
  auto* comp = findTopOverlayAt(components_, mx, my);
  if (comp == nullptr) {
    comp = resolveTreeHit(*this, mx, my);
  }
  return comp;
}

bool GuiWidgetTree::handleMouseDownHit(GuiWidget& comp,
                                       const GuiMouseEvent& event) {
  if (comp.handleMouseDown(event)) {
    captured_ = &comp;
    beginTextInputDrag(&comp, event.x);
    return true;
  }
  pending_click_target_ = &comp;
  pending_click_button_ = event.button;
  beginTextInputDrag(&comp, event.x);
  return true;
}

bool GuiWidgetTree::dispatchMouseDown(const GuiMouseEvent& event) {
  pending_click_target_ = nullptr;
  drag_moved_ = false;
  auto* comp = hitTestAny(event.x, event.y);
  if (comp == nullptr) {
    drag_input_ = nullptr;
    return false;
  }
  return handleMouseDownHit(*comp, event);
}

void GuiWidgetTree::dispatchMouseUp(const GuiMouseEvent& event) {
  auto drag_mode =
      drag_moved_ ? GuiSelectionExtend::EXTEND : GuiSelectionExtend::COLLAPSE;
  drag_input_ = nullptr;
  drag_moved_ = false;
  if (captured_ != nullptr) {
    captured_->handleMouseUp(event);
    captured_ = nullptr;
    pending_click_target_ = nullptr;
    return;
  }
  tryFireClickFromMouseUp(event, drag_mode);
}

void GuiWidgetTree::dispatchMouseMove(const GuiMouseEvent& event) {
  if (captured_ != nullptr) {
    captured_->handleMouseMove(event);
    return;
  }
  if (drag_input_ != nullptr) {
    drag_input_->setCursorClickX(event.x, GuiSelectionExtend::EXTEND);
    drag_moved_ = true;
  }
  updateHover(event.x, event.y);
}

bool GuiWidgetTree::dispatchScroll(const GuiScrollEvent& event) {
  auto* comp = findTopOverlayAt(components_, event.x, event.y);
  if (comp == nullptr) {
    comp = resolveTreeHit(*this, event.x, event.y);
  }
  if (comp == nullptr) {
    return false;
  }
  return comp->handleScroll(event);
}

bool GuiWidgetTree::hasCapture() const {
  return captured_ != nullptr;
}

bool GuiWidgetTree::dispatchText(std::string_view text) {
  if (focused_input_ == nullptr) {
    return false;
  }
  focused_input_->insertAtCursor(text);
  return true;
}

bool GuiWidgetTree::dispatchKey(uint32_t keycode) {
  GuiKeyEvent event{};
  event.keycode = keycode;
  event.pressed = true;
  return dispatchKey(event);
}

bool GuiWidgetTree::handleTextNavKey(GuiTextInput& input,
                                     const GuiKeyEvent& event) {
  switch (event.keycode) {
    case SDLK_LEFT:
    case SDLK_RIGHT:
    case SDLK_UP:
    case SDLK_DOWN:
    case SDLK_HOME:
    case SDLK_END:
      input.moveCursor(event);
      return true;
    default:
      return false;
  }
}

bool GuiWidgetTree::handleReturnKey(GuiTextInput& input) {
  if (input.widget_type != GuiWidgetType::TEXT_AREA) {
    return false;
  }
  input.insertAtCursor("\n");
  return true;
}

bool GuiWidgetTree::handleDeletionKey(GuiTextInput& input, uint32_t keycode) {
  if (keycode == SDLK_BACKSPACE) {
    input.deleteBack();
    return true;
  }
  if (keycode == SDLK_DELETE) {
    input.deleteForward();
    return true;
  }
  return false;
}

bool GuiWidgetTree::handleTextEditKey(GuiTextInput& input,
                                      const GuiKeyEvent& event) {
  if (event.keycode == SDLK_RETURN) {
    return handleReturnKey(input);
  }
  if (handleDeletionKey(input, event.keycode)) {
    return true;
  }
  if ((event.ctrl || event.gui) && event.keycode == SDLK_A) {
    input.selectAll();
    return true;
  }
  return handleClipboardKey(input, event);
}

void GuiWidgetTree::cutToClipboard(GuiTextInput& input) {
  copyToClipboard(input);
  if (input.hasSelection()) {
    input.deleteBack();
  }
}

bool GuiWidgetTree::dispatchClipboardAction(GuiTextInput& input,
                                            uint32_t keycode) {
  if (keycode == SDLK_C) {
    copyToClipboard(input);
    return true;
  }
  if (keycode == SDLK_X) {
    cutToClipboard(input);
    return true;
  }
  if (keycode == SDLK_V) {
    pasteFromClipboard(input);
    return true;
  }
  return false;
}

bool GuiWidgetTree::handleClipboardKey(GuiTextInput& input,
                                       const GuiKeyEvent& event) {
  if (!event.ctrl && !event.gui) {
    return false;
  }
  return dispatchClipboardAction(input, event.keycode);
}

void GuiWidgetTree::copyToClipboard(const GuiTextInput& input) {
  if (!input.hasSelection()) {
    return;
  }
  auto sel = input.selectedText();
  std::string text(sel);
  SDL_SetClipboardText(text.c_str());
}

void GuiWidgetTree::pasteFromClipboard(GuiTextInput& input) {
  // NOLINTBEGIN(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc,hicpp-no-malloc)
  // -- SDL allocates; must use SDL_free
  char* text = SDL_GetClipboardText();
  if (text != nullptr && text[0] != '\0') {
    input.insertAtCursor(text);
  }
  SDL_free(text);
  // NOLINTEND(cppcoreguidelines-owning-memory,cppcoreguidelines-no-malloc,hicpp-no-malloc)
}

bool GuiWidgetTree::dispatchKey(const GuiKeyEvent& event) {
  if (focused_input_ == nullptr) {
    return false;
  }
  if (handleTextNavKey(*focused_input_, event)) {
    return true;
  }
  return handleTextEditKey(*focused_input_, event);
}

bool GuiWidgetTree::hasFocusedInput() const {
  return focused_input_ != nullptr;
}

void GuiWidgetTree::clearFocus() {
  if (focused_input_ != nullptr) {
    focused_input_->focus = GuiTextInputFocus::UNFOCUSED;
  }
  focused_input_ = nullptr;
}

size_t GuiWidgetTree::componentCount() const {
  return components_.size();
}

}  // namespace eng
