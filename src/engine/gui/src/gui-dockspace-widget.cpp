/// @file gui-dockspace-widget.cpp
/// @brief Implementation of `GuiDockspaceWidget`. See the header for the
/// public contract and docs/technical-approaches/engine/gui/dockspace.md
/// for the Phase 1 design.

#include "engine/gui/gui-dockspace-widget.h"

#include "engine/core/assert.h"
#include "engine/core/logger.h"
#include "engine/gui/dockspace-arrange.h"
#include "engine/gui/gui-widget-tree.h"
#include "engine/gui/gui-widget-type.h"

#include <cstddef>
#include <utility>

namespace eng {

namespace {

  /// Subsystem tag used for logger calls from the dockspace.
  constexpr const char* LOG_SUBSYSTEM = "Dockspace";

  /// True if `child` is already bound to a region with an index other
  /// than `target_idx`. `GUI_WIDGET_ID_INVALID` never conflicts.
  bool childAssignedElsewhere(const GuiDockLayout& layout, GuiWidgetId child,
                              size_t target_idx) {
    if (child == GUI_WIDGET_ID_INVALID) {
      return false;
    }
    for (size_t i = 0; i < DOCK_EDGE_COUNT; ++i) {
      if (i == target_idx) {
        continue;
      }
      if (layout.regions[i].child == child) {
        return true;
      }
    }
    return false;
  }

  /// True if `child_id` appears in any region of `layout`.
  bool childReferencedByLayout(const GuiDockLayout& layout,
                               GuiWidgetId child_id) {
    for (size_t i = 0; i < DOCK_EDGE_COUNT; ++i) {
      if (layout.regions[i].child == child_id) {
        return true;
      }
    }
    return false;
  }

}  // namespace

GuiDockspaceWidget::GuiDockspaceWidget(GuiDockLayout layout)
  : layout_(std::move(layout)) {
  widget_type = GuiWidgetType::CUSTOM;
  debug_name = "Dockspace";
}

bool GuiDockspaceWidget::assignChild(DockEdge edge, GuiWidgetId child) {
  auto idx = static_cast<size_t>(edge);
  if (childAssignedElsewhere(layout_, child, idx)) {
    Logger::error(LOG_SUBSYSTEM, "child already assigned to another region");
    return false;
  }
  layout_.regions[idx].edge = edge;
  layout_.regions[idx].child = child;
  return true;
}

void GuiDockspaceWidget::clearRegion(DockEdge edge) {
  auto idx = static_cast<size_t>(edge);
  layout_.regions[idx].edge = edge;
  layout_.regions[idx].child = GUI_WIDGET_ID_INVALID;
}

std::optional<Rect> GuiDockspaceWidget::regionRect(DockEdge edge) const {
  auto idx = static_cast<size_t>(edge);
  if (!has_layout_) {
    return std::nullopt;
  }
  if (layout_.regions[idx].child == GUI_WIDGET_ID_INVALID) {
    return std::nullopt;
  }
  return region_rects_[idx];
}

void GuiDockspaceWidget::setLayout(GuiDockLayout layout) {
  layout_ = std::move(layout);
  has_layout_ = false;
  warned_unassigned_.clear();
}

const GuiDockLayout& GuiDockspaceWidget::layout() const {
  return layout_;
}

std::unique_ptr<GuiWidget> GuiDockspaceWidget::clone() const {
  return std::make_unique<GuiDockspaceWidget>(*this);
}

void GuiDockspaceWidget::render(const GuiDrawContext& /*ctx*/) const {
  // Dockspace is invisible. Children render themselves in tree draw order.
}

void GuiDockspaceWidget::arrangeChildren(GuiWidgetTree& tree,
                                         const Rect& available) {
  bool centre_has_area = computeDockRegions(layout_, available, region_rects_);
  has_layout_ = true;
  if (!centre_has_area) {
    Logger::warn(LOG_SUBSYSTEM,
                 "centre region clamped to zero (viewport too small)");
  }
  for (size_t i = 0; i < DOCK_EDGE_COUNT; ++i) {
    const auto& region = layout_.regions[i];
    if (region.child == GUI_WIDGET_ID_INVALID) {
      continue;
    }
    tree.arrangeWidget(region.child, region_rects_[i]);
  }
  warnUnassignedChildren();
  assertNoOverlaps();
}

void GuiDockspaceWidget::warnUnassignedChildren() {
  for (auto child_id : children) {
    if (childReferencedByLayout(layout_, child_id)) {
      continue;
    }
    if (warned_unassigned_.contains(child_id)) {
      continue;
    }
    Logger::warn(LOG_SUBSYSTEM,
                 "tree child has no dock region assignment — panel will "
                 "keep a stale rect until assignChild is called");
    warned_unassigned_.insert(child_id);
  }
}

void GuiDockspaceWidget::assertNoOverlaps() const {
#ifndef NDEBUG
  for (size_t i = 0; i < DOCK_EDGE_COUNT; ++i) {
    if (layout_.regions[i].child == GUI_WIDGET_ID_INVALID) {
      continue;
    }
    for (size_t j = i + 1; j < DOCK_EDGE_COUNT; ++j) {
      if (layout_.regions[j].child == GUI_WIDGET_ID_INVALID) {
        continue;
      }
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while)
      ENGINE_ASSERT(!rectsOverlap(region_rects_[i], region_rects_[j]),
                    "dockspace regions overlap — layout invariant violated");
    }
  }
#endif
}

}  // namespace eng
