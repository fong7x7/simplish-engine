#include "flex-layout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace eng {

namespace {

  /// Differences smaller than this are rounding, not space.
  constexpr float LAYOUT_EPSILON = 0.001f;

  /// A screen axis.
  enum class Axis : uint8_t {
    /// Horizontal.
    X,
    /// Vertical.
    Y,
  };

  /// Where something starts along an axis, and how long it is.
  struct Span {
    /// Start, in logical pixels.
    float start = 0.0f;
    /// Length, in logical pixels.
    float length = 0.0f;
  };

  /// How free space is handed out: before the first item, and between
  /// each pair.
  struct Distribution {
    /// Space before the first.
    float lead = 0.0f;
    /// Extra space between each pair, on top of the gap.
    float between = 0.0f;
  };

  /// One in-flow child, as the arrange pass works on it.
  struct FlexItem {
    /// The child's tree id.
    GuiWidgetId id = GUI_WIDGET_ID_INVALID;
    /// Its layout style, copied so arranging siblings cannot move it.
    LayoutStyle style{};
    /// Hypothetical main size: the basis, within min and max.
    float base = 0.0f;
    /// Resolved main size, after growing or shrinking.
    float main = 0.0f;
    /// Hypothetical cross size: the measured one.
    float cross = 0.0f;
    /// How far its limits moved it from its share of the free space on
    /// the last round of flexing: positive when its min held it up.
    float held = 0.0f;
    /// Whether flexing has settled this item's main size.
    bool frozen = false;
  };

  /// Whether a line's items grow to fill it or shrink to fit it.
  enum class FlexMode : uint8_t {
    /// The items leave room: they grow by their grow factors.
    GROW,
    /// The items overflow: they shrink by their shrink factors.
    SHRINK,
  };

  /// A run of items on one line of a container.
  struct FlexLine {
    /// Index of the line's first item.
    size_t begin = 0;
    /// One past the index of its last.
    size_t end = 0;
    /// The line's cross size.
    float cross = 0.0f;
    /// Where it starts across, from the content box's cross start.
    float offset = 0.0f;
  };

  /// What every step of arranging one container needs.
  struct FlexFrame {
    /// The container's style.
    const LayoutStyle& style;
    /// Its main axis.
    Axis main = Axis::X;
    /// Its content box: the border box less the padding.
    Rect content{};
  };

  Axis mainAxisOf(FlexDirection direction) {
    return direction == FlexDirection::ROW ? Axis::X : Axis::Y;
  }

  Axis otherAxis(Axis axis) {
    return axis == Axis::X ? Axis::Y : Axis::X;
  }

  float along(const LayoutSize& size, Axis axis) {
    return axis == Axis::X ? size.w : size.h;
  }

  Span spanOf(const Rect& rect, Axis axis) {
    return axis == Axis::X ? Span{rect.x, rect.w} : Span{rect.y, rect.h};
  }

  /// A size with @p main along @p main_axis and @p cross across it.
  LayoutSize sizeFrom(Axis main_axis, float main, float cross) {
    return main_axis == Axis::X ? LayoutSize{main, cross}
                                : LayoutSize{cross, main};
  }

  /// A rect spanning @p main along @p main_axis and @p cross across it.
  Rect rectFrom(Axis main_axis, Span main, Span cross) {
    return main_axis == Axis::X
               ? Rect{main.start, cross.start, main.length, cross.length}
               : Rect{cross.start, main.start, cross.length, main.length};
  }

  float leading(const Edges& edges, Axis axis) {
    return axis == Axis::X ? edges.left : edges.top;
  }

  float trailing(const Edges& edges, Axis axis) {
    return axis == Axis::X ? edges.right : edges.bottom;
  }

  float edgesAlong(const Edges& edges, Axis axis) {
    return leading(edges, axis) + trailing(edges, axis);
  }

  float explicitAlong(const LayoutStyle& style, Axis axis) {
    return axis == Axis::X ? style.width : style.height;
  }

  /// @p value within @p style's min and max along @p axis — the min
  /// winning when they cross — and never negative.
  float clampAlong(const LayoutStyle& style, Axis axis, float value) {
    const float lo = axis == Axis::X ? style.min_width : style.min_height;
    const float hi = axis == Axis::X ? style.max_width : style.max_height;
    const float capped = hi >= 0.0f ? std::min(value, hi) : value;
    return std::max({capped, lo, 0.0f});
  }

  Rect contentBox(const Rect& box, const Edges& padding) {
    return {box.x + padding.left, box.y + padding.top,
            std::max(0.0f, box.w - edgesAlong(padding, Axis::X)),
            std::max(0.0f, box.h - edgesAlong(padding, Axis::Y))};
  }

  bool inFlow(const GuiWidget& child) {
    return child.visible &&
           child.tree_layout.position == PositionMode::RELATIVE;
  }

  /// @p child's hypothetical main size: its basis, within its min and max.
  float baseSize(const GuiWidget& child, Axis main) {
    const LayoutStyle& style = child.tree_layout;
    const float basis = style.flex_basis >= 0.0f
                            ? style.flex_basis
                            : along(child.tree_measured, main);
    return clampAlong(style, main, basis);
  }

  /// @p item's size along @p axis plus its margins there.
  float outer(const FlexItem& item, Axis axis, float size) {
    return size + edgesAlong(item.style.margin, axis);
  }

  /// The space @p count items leave between them for the gap.
  float gapsFor(size_t count, float gap) {
    return count > 1 ? gap * static_cast<float>(count - 1) : 0.0f;
  }

  /// @p child's size along @p main and across it, margins included.
  LayoutSize outerMeasured(const GuiWidget& child, Axis main) {
    const Edges& margin = child.tree_layout.margin;
    const Axis cross = otherAxis(main);
    return sizeFrom(main, baseSize(child, main) + edgesAlong(margin, main),
                    along(child.tree_measured, cross) +
                        edgesAlong(margin, cross));
  }

  /// In-flow content size of @p widget: its children end to end along its
  /// main axis, gaps between, and the thickest across.
  LayoutSize flowContent(const GuiWidgetTree& tree, const GuiWidget& widget) {
    const Axis main = mainAxisOf(widget.tree_layout.direction);
    float sum = 0.0f;
    float widest = 0.0f;
    size_t count = 0;
    for (const GuiWidgetId id : widget.children) {
      const GuiWidget* child = tree.findWidget(id);
      if (child != nullptr && inFlow(*child)) {
        const LayoutSize size = outerMeasured(*child, main);
        sum += along(size, main);
        widest = std::max(widest, along(size, otherAxis(main)));
        ++count;
      }
    }
    return sizeFrom(main, sum + gapsFor(count, widget.tree_layout.gap), widest);
  }

  /// One axis of `measureBorderBox`.
  float measureAlong(const LayoutStyle& style, Axis axis, float content) {
    const float size = explicitAlong(style, axis);
    const float natural = content + edgesAlong(style.padding, axis);
    return clampAlong(style, axis, size >= 0.0f ? size : natural);
  }

  /// The in-flow children of @p parent, with their hypothetical sizes.
  std::vector<FlexItem> collectItems(const GuiWidgetTree& tree,
                                     const GuiWidget& parent, Axis main) {
    std::vector<FlexItem> items;
    items.reserve(parent.children.size());
    for (const GuiWidgetId id : parent.children) {
      const GuiWidget* child = tree.findWidget(id);
      if (child == nullptr || !inFlow(*child)) {
        continue;
      }
      const float base = baseSize(*child, main);
      items.push_back({.id = id,
                       .style = child->tree_layout,
                       .base = base,
                       .main = base,
                       .cross = along(child->tree_measured, otherAxis(main))});
    }
    return items;
  }

  /// Whether @p size more, after a gap, overflows @p line, @p used long.
  bool overflows(const FlexLine& line, float used, float size,
                 const FlexFrame& frame) {
    return frame.style.wrap == FlexWrap::WRAP && line.end > line.begin &&
           used + frame.style.gap + size >
               spanOf(frame.content, frame.main).length + LAYOUT_EPSILON;
  }

  /// @p items broken into lines that fit the content box's main size, or
  /// one line when the container does not wrap.
  std::vector<FlexLine> breakLines(const std::vector<FlexItem>& items,
                                   const FlexFrame& frame) {
    std::vector<FlexLine> lines{{.begin = 0, .end = 0}};
    float used = 0.0f;
    for (size_t i = 0; i < items.size(); ++i) {
      const float size = outer(items[i], frame.main, items[i].base);
      if (overflows(lines.back(), used, size, frame)) {
        lines.push_back({.begin = i, .end = i});
      }
      FlexLine& line = lines.back();
      used = (line.end > line.begin ? used + frame.style.gap : 0.0f) + size;
      line.end = i + 1;
    }
    return lines;
  }

  /// How strongly @p item flexes: its grow factor when growing, its
  /// shrink factor scaled by its base when shrinking.
  float flexFactor(const FlexItem& item, FlexMode mode) {
    return mode == FlexMode::GROW ? item.style.flex_grow
                                  : item.style.flex_shrink * item.base;
  }

  /// The main-axis space @p line has for its items' border boxes: the
  /// content box less the gaps and every item's margins.
  float innerRoom(const std::vector<FlexItem>& items, const FlexLine& line,
                  const FlexFrame& frame) {
    float room = spanOf(frame.content, frame.main).length -
                 gapsFor(line.end - line.begin, frame.style.gap);
    for (size_t i = line.begin; i < line.end; ++i) {
      room -= edgesAlong(items[i].style.margin, frame.main);
    }
    return room;
  }

  /// Space left on @p line when frozen items take their resolved size and
  /// the rest their base.
  float freeSpace(const std::vector<FlexItem>& items, const FlexLine& line,
                  float room) {
    float free_space = room;
    for (size_t i = line.begin; i < line.end; ++i) {
      free_space -= items[i].frozen ? items[i].main : items[i].base;
    }
    return free_space;
  }

  float sumFactors(const std::vector<FlexItem>& items, const FlexLine& line,
                   FlexMode mode) {
    float sum = 0.0f;
    for (size_t i = line.begin; i < line.end; ++i) {
      sum += items[i].frozen ? 0.0f : flexFactor(items[i], mode);
    }
    return sum;
  }

  /// One round of flexing a line.
  struct FlexRound {
    /// The space the line's unfrozen items share.
    float free_space = 0.0f;
    /// Whether they grow or shrink.
    FlexMode mode = FlexMode::GROW;
    /// The container's main axis.
    Axis main = Axis::X;
  };

  /// The part of @p round's free space handed out: all of it, except that
  /// grow factors summing below one hand out only that fraction, as CSS.
  float shareOf(const FlexRound& round, float factors) {
    return round.mode == FlexMode::GROW && factors < 1.0f
               ? round.free_space * factors
               : round.free_space;
  }

  /// Give each unfrozen item on @p line its share of the free space,
  /// within its limits, noting how far the limits moved it. Returns the
  /// total they moved.
  float shareFreeSpace(std::vector<FlexItem>& items, const FlexLine& line,
                       const FlexRound& round) {
    const float factors = sumFactors(items, line, round.mode);
    const float per_factor = shareOf(round, factors) / factors;
    float violation = 0.0f;
    for (size_t i = line.begin; i < line.end; ++i) {
      FlexItem& item = items[i];
      if (!item.frozen) {
        const float target =
            item.base + per_factor * flexFactor(item, round.mode);
        item.main = clampAlong(item.style, round.main, target);
        item.held = item.main - target;
        violation += item.held;
      }
    }
    return violation;
  }

  /// Freeze the unfrozen items on @p line whose limits held them the way
  /// @p violation says most did — or every one, when none were held.
  void freezeViolators(std::vector<FlexItem>& items, const FlexLine& line,
                       float violation) {
    for (size_t i = line.begin; i < line.end; ++i) {
      FlexItem& item = items[i];
      const bool settles = std::abs(violation) < LAYOUT_EPSILON ||
                           (violation > 0.0f && item.held > 0.0f) ||
                           (violation < 0.0f && item.held < 0.0f);
      item.frozen = item.frozen || settles;
    }
  }

  /// Start flexing @p line: every item at its base, and those that do not
  /// flex in @p mode frozen there.
  void freezeInflexible(std::vector<FlexItem>& items, const FlexLine& line,
                        FlexMode mode) {
    for (size_t i = line.begin; i < line.end; ++i) {
      FlexItem& item = items[i];
      item.main = item.base;
      item.frozen = flexFactor(item, mode) <= 0.0f;
    }
  }

  bool allFrozen(const std::vector<FlexItem>& items, const FlexLine& line) {
    return std::all_of(items.begin() + static_cast<std::ptrdiff_t>(line.begin),
                       items.begin() + static_cast<std::ptrdiff_t>(line.end),
                       [](const FlexItem& item) { return item.frozen; });
  }

  /// Grow or shrink the items on @p line to fill it: CSS's resolving of
  /// flexible lengths, freezing items as their min or max holds them. Each
  /// round freezes at least one item, so it ends.
  void resolveLengths(std::vector<FlexItem>& items, const FlexLine& line,
                      const FlexFrame& frame) {
    const float room = innerRoom(items, line, frame);
    for (size_t i = line.begin; i < line.end; ++i) {
      items[i].frozen = false;
    }
    const float initial = freeSpace(items, line, room);
    const FlexMode mode = initial > 0.0f ? FlexMode::GROW : FlexMode::SHRINK;
    freezeInflexible(items, line, mode);
    while (!allFrozen(items, line)) {
      const FlexRound round{.free_space = freeSpace(items, line, room),
                            .mode = mode,
                            .main = frame.main};
      freezeViolators(items, line, shareFreeSpace(items, line, round));
    }
  }

  /// The space-* alignments: @p edge_parts gaps' worth at each edge for
  /// every one between — 0 for space-between, a half for space-around, 1
  /// for space-evenly. Without room, space-between packs at the start and
  /// the others centre.
  Distribution spread(float free_space, size_t count, float edge_parts) {
    if (edge_parts == 0.0f && (free_space <= 0.0f || count < 2)) {
      return {};
    }
    if (free_space <= 0.0f || count == 0) {
      return {free_space * 0.5f, 0.0f};
    }
    const float slots = static_cast<float>(count - 1) + edge_parts * 2.0f;
    const float between = free_space / slots;
    return {between * edge_parts, between};
  }

  /// Hand @p free_space out by @p align among @p count things.
  Distribution distribute(Align align, float free_space, size_t count) {
    switch (align) {
      case Align::CENTER:
        return {free_space * 0.5f, 0.0f};
      case Align::END:
        return {free_space, 0.0f};
      case Align::SPACE_BETWEEN:
        return spread(free_space, count, 0.0f);
      case Align::SPACE_AROUND:
        return spread(free_space, count, 0.5f);
      case Align::SPACE_EVENLY:
        return spread(free_space, count, 1.0f);
      default:
        return {};
    }
  }

  /// The alignment @p item takes across its line.
  Align crossAlignOf(const FlexItem& item, const FlexFrame& frame) {
    return item.style.align_self == Align::AUTO ? frame.style.align_items
                                                : item.style.align_self;
  }

  /// Give each line its cross size: the thickest item on it, or the whole
  /// content box for a single line that does not wrap. Returns the space
  /// the lines and the gaps between them leave across the content box.
  float sizeLines(const std::vector<FlexItem>& items,
                  std::vector<FlexLine>& lines, const FlexFrame& frame) {
    const Axis cross = otherAxis(frame.main);
    const float room = spanOf(frame.content, cross).length;
    if (frame.style.wrap == FlexWrap::NO_WRAP) {
      lines.front().cross = room;
      return 0.0f;
    }
    float free_space = room - gapsFor(lines.size(), frame.style.gap);
    for (FlexLine& line : lines) {
      for (size_t i = line.begin; i < line.end; ++i) {
        line.cross =
            std::max(line.cross, outer(items[i], cross, items[i].cross));
      }
      free_space -= line.cross;
    }
    return free_space;
  }

  /// Offset each line across by `align_content`, stretching them to share
  /// @p free_space when it says STRETCH.
  void alignLines(std::vector<FlexLine>& lines, float free_space,
                  const FlexFrame& frame) {
    const Align align = frame.style.align_content;
    if (align == Align::STRETCH && free_space > 0.0f) {
      for (FlexLine& line : lines) {
        line.cross += free_space / static_cast<float>(lines.size());
      }
    }
    const Distribution d = distribute(align, free_space, lines.size());
    float at = d.lead;
    for (FlexLine& line : lines) {
      line.offset = at;
      at += line.cross + frame.style.gap + d.between;
    }
  }

  /// How far into @p room an item @p size thick starts, aligned by
  /// @p align.
  float alignWithin(Align align, float room, float size) {
    if (align == Align::CENTER) {
      return (room - size) * 0.5f;
    }
    return align == Align::END ? room - size : 0.0f;
  }

  /// Where @p item sits across @p line, and how thick it is.
  Span crossSpan(const FlexItem& item, const FlexLine& line,
                 const FlexFrame& frame) {
    const Axis cross = otherAxis(frame.main);
    const Align align = crossAlignOf(item, frame);
    const float room = line.cross - edgesAlong(item.style.margin, cross);
    const bool stretches =
        align == Align::STRETCH && explicitAlong(item.style, cross) < 0.0f;
    const float size =
        stretches ? clampAlong(item.style, cross, room) : item.cross;
    return {spanOf(frame.content, cross).start + line.offset +
                leading(item.style.margin, cross) +
                alignWithin(align, room, size),
            size};
  }

  /// The main-axis space @p line's items leave unused.
  float lineFreeSpace(const std::vector<FlexItem>& items, const FlexLine& line,
                      const FlexFrame& frame) {
    float free_space = innerRoom(items, line, frame);
    for (size_t i = line.begin; i < line.end; ++i) {
      free_space -= items[i].main;
    }
    return free_space;
  }

  /// Arrange each item on @p line in turn along the main axis.
  void placeLine(GuiWidgetTree& tree, const std::vector<FlexItem>& items,
                 const FlexLine& line, const FlexFrame& frame) {
    const Distribution d =
        distribute(frame.style.justify_content,
                   lineFreeSpace(items, line, frame), line.end - line.begin);
    float at = spanOf(frame.content, frame.main).start + d.lead;
    for (size_t i = line.begin; i < line.end; ++i) {
      const FlexItem& item = items[i];
      at += leading(item.style.margin, frame.main);
      const Span cross = crossSpan(item, line, frame);
      tree.arrangeWidget(item.id, rectFrom(frame.main, {at, item.main}, cross));
      at += item.main + trailing(item.style.margin, frame.main) +
            frame.style.gap + d.between;
    }
  }

  /// Where an absolute @p child sits along @p axis of @p box, and how long
  /// it is: stretched between its insets when both are set and its size is
  /// auto, anchored to the far one when only that is, else at the near one
  /// and its measured size.
  Span absoluteSpan(const GuiWidget& child, Axis axis, Span box) {
    const LayoutStyle& style = child.tree_layout;
    const float near = axis == Axis::X ? style.abs_x : style.abs_y;
    const float far = axis == Axis::X ? style.abs_right : style.abs_bottom;
    const float lead = leading(style.margin, axis);
    const float room = box.length - near - far - edgesAlong(style.margin, axis);
    if (far >= 0.0f && explicitAlong(style, axis) < 0.0f) {
      return {box.start + near + lead, clampAlong(style, axis, room)};
    }
    const float size = along(child.tree_measured, axis);
    if (far >= 0.0f) {
      return {box.start + box.length - far - trailing(style.margin, axis) -
                  size,
              size};
    }
    return {box.start + near + lead, size};
  }

  /// Arrange @p parent's absolute children within @p box — hidden ones
  /// too, so an overlay shown later is already where it belongs.
  void placeAbsolute(GuiWidgetTree& tree, const GuiWidget& parent,
                     const Rect& box) {
    for (const GuiWidgetId id : parent.children) {
      const GuiWidget* child = tree.findWidget(id);
      if (child == nullptr ||
          child->tree_layout.position != PositionMode::ABSOLUTE) {
        continue;
      }
      const Span x = absoluteSpan(*child, Axis::X, spanOf(box, Axis::X));
      const Span y = absoluteSpan(*child, Axis::Y, spanOf(box, Axis::Y));
      tree.arrangeWidget(id, rectFrom(Axis::X, x, y));
    }
  }

}  // namespace

LayoutSize measureBorderBox(const GuiWidgetTree& tree, const GuiWidget& widget,
                            const GuiDrawContext& ctx) {
  const LayoutSize own = widget.measureContent(ctx);
  const LayoutSize flow = flowContent(tree, widget);
  const LayoutStyle& style = widget.tree_layout;
  return {measureAlong(style, Axis::X, std::max(own.w, flow.w)),
          measureAlong(style, Axis::Y, std::max(own.h, flow.h))};
}

void arrangeFlexChildren(GuiWidgetTree& tree, const GuiWidget& parent,
                         const Rect& box) {
  const FlexFrame frame{.style = parent.tree_layout,
                        .main = mainAxisOf(parent.tree_layout.direction),
                        .content = contentBox(box, parent.tree_layout.padding)};
  std::vector<FlexItem> items = collectItems(tree, parent, frame.main);
  if (!items.empty()) {
    std::vector<FlexLine> lines = breakLines(items, frame);
    for (const FlexLine& line : lines) {
      resolveLengths(items, line, frame);
    }
    alignLines(lines, sizeLines(items, lines, frame), frame);
    for (const FlexLine& line : lines) {
      placeLine(tree, items, line, frame);
    }
  }
  placeAbsolute(tree, parent, box);
}

}  // namespace eng
