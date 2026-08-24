/// @file markdown-renderer.cpp
/// @brief Converts Markdown block AST into GuiWidgetTree subtree.

#include <cstddef>
#include <engine/gui/gui-label.h>
#include <engine/gui/gui-panel.h>
#include <engine/gui/gui-widget-tree.h>
#include <engine/gui/gui-widget-type.h>
#include <engine/gui/markdown-column-align.h>
#include <engine/gui/markdown-renderer.h>
#include <engine/gui/rich-text.h>
#include <engine/gui/text-span.h>
#include <string>
#include <vector>

namespace eng::gui {
namespace {

  const MarkdownRenderConfig DEFAULT_CONFIG{};

  const MarkdownRenderConfig& resolveConfig(const MarkdownRenderConfig* cfg) {
    return cfg != nullptr ? *cfg : DEFAULT_CONFIG;
  }

  // ---------------------------------------------------------------------------
  // RichText builder
  // ---------------------------------------------------------------------------

  uint32_t packColor(const GuiColor& c) {
    return c.pack();
  }

  struct InlineRenderCtx {
    /// Config for colour/size lookups.
    const MarkdownRenderConfig& config;
    /// Font size override (0 = use base).
    float font_size = 0.0f;
  };

  TextSpan makeSpan(uint32_t start, uint32_t end, TextStyle style,
                    uint32_t color) {
    return TextSpan{start, end, style, color, FONT_SIZE_INHERIT};
  }

  struct InlineStyle {
    /// Text style for the inline type.
    TextStyle style = TextStyle::NORMAL;
    /// Packed RGBA colour for the inline type.
    uint32_t color = 0xFFFFFFFF;
  };

  InlineStyle resolveInlineStyle(MarkdownInlineType type,
                                 const InlineRenderCtx& ctx) {
    auto text_col = packColor(ctx.config.text_color);
    switch (type) {
      case MarkdownInlineType::BOLD:
        return {TextStyle::BOLD, text_col};
      case MarkdownInlineType::ITALIC:
        return {TextStyle::ITALIC, text_col};
      case MarkdownInlineType::BOLD_ITALIC:
        return {TextStyle::BOLD_ITALIC, text_col};
      case MarkdownInlineType::INLINE_CODE:
        return {TextStyle::CODE, packColor(ctx.config.code_text_color)};
      case MarkdownInlineType::LINK:
        return {TextStyle::NORMAL, packColor(ctx.config.link_color)};
      default:
        return {TextStyle::NORMAL, text_col};
    }
  }

  void appendInline(RichText& rt, const MarkdownInline& inl,
                    const InlineRenderCtx& ctx) {
    auto start = static_cast<uint32_t>(rt.text.size());
    rt.text += inl.text;
    auto end_pos = static_cast<uint32_t>(rt.text.size());
    auto s = resolveInlineStyle(inl.type, ctx);
    rt.spans.push_back(makeSpan(start, end_pos, s.style, s.color));
  }

  RichText buildRichText(const std::vector<MarkdownInline>& inlines,
                         const InlineRenderCtx& ctx) {
    RichText rt;
    for (const auto& inl : inlines) {
      appendInline(rt, inl, ctx);
    }
    return rt;
  }

  // ---------------------------------------------------------------------------
  // Widget helpers
  // ---------------------------------------------------------------------------

  GuiWidgetId createPanel(GuiWidgetTree& tree, GuiWidgetId parent) {
    return tree.createWidget(GuiWidgetType::PANEL, parent);
  }

  GuiWidgetId createLabel(GuiWidgetTree& tree, GuiWidgetId parent) {
    return tree.createWidget(GuiWidgetType::TEXT, parent);
  }

  GuiWidgetId createScroll(GuiWidgetTree& tree, GuiWidgetId parent) {
    return tree.createWidget(GuiWidgetType::SCROLL_CONTAINER, parent);
  }

  void configurePanel(GuiWidgetTree& tree, GuiWidgetId id,
                      const GuiColor& fill) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    auto* panel = dynamic_cast<GuiPanel*>(w);
    if (panel != nullptr) {
      panel->fill_color = fill;
    }
  }

  void setLayout(GuiWidgetTree& tree, GuiWidgetId id, FlexDirection dir,
                 float gap) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    w->tree_layout.direction = dir;
    w->tree_layout.gap = gap;
  }

  void setFlexGrow(GuiWidgetTree& tree, GuiWidgetId id, float grow) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    w->tree_layout.flex_grow = grow;
  }

  void setWidthConstraints(GuiWidgetTree& tree, GuiWidgetId id, float min_w,
                           float max_w) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    w->tree_layout.min_width = min_w;
    w->tree_layout.max_width = max_w;
  }

  void setPadding(GuiWidgetTree& tree, GuiWidgetId id, float padding) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    w->tree_layout.padding = {padding, padding, padding, padding};
  }

  void setDebugName(GuiWidgetTree& tree, GuiWidgetId id,
                    std::string_view name) {
    auto* w = tree.findWidget(id);
    if (w != nullptr) {
      w->debug_name = std::string(name);
    }
  }

  void configureCellBorder(GuiWidgetTree& tree, GuiWidgetId id,
                           const MarkdownRenderConfig& config) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    auto* panel = dynamic_cast<GuiPanel*>(w);
    if (panel == nullptr) {
      return;
    }
    panel->border_color = config.table_border_color;
    panel->border_width = config.table_border_width;
  }

  void setLabelText(GuiWidgetTree& tree, GuiWidgetId id, const RichText& rich) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    auto* label = dynamic_cast<GuiLabel*>(w);
    if (label != nullptr) {
      label->text = rich.text;
    }
  }

  GuiLabelAlign mapAlignment(MarkdownColumnAlign align) {
    switch (align) {
      case MarkdownColumnAlign::CENTER:
        return GuiLabelAlign::H_CENTER;
      case MarkdownColumnAlign::RIGHT:
        return GuiLabelAlign::LEFT;
      case MarkdownColumnAlign::LEFT:
        return GuiLabelAlign::LEFT;
    }
    return GuiLabelAlign::LEFT;
  }

  // ---------------------------------------------------------------------------
  // Block renderers
  // ---------------------------------------------------------------------------

  void renderParagraph(GuiWidgetTree& tree, GuiWidgetId parent,
                       const MarkdownBlock& block,
                       const MarkdownRenderConfig& config) {
    InlineRenderCtx ctx{config};
    auto rt = buildRichText(block.inlines, ctx);
    auto label_id = createLabel(tree, parent);
    setLabelText(tree, label_id, rt);
    setDebugName(tree, label_id, "md_paragraph");
  }

  void renderHeading(GuiWidgetTree& tree, GuiWidgetId parent,
                     const MarkdownBlock& block,
                     const MarkdownRenderConfig& config) {
    InlineRenderCtx ctx{config};
    auto rt = buildRichText(block.inlines, ctx);
    auto label_id = createLabel(tree, parent);
    setLabelText(tree, label_id, rt);
    setDebugName(tree, label_id, "md_heading");
  }

  void renderCodeBlock(GuiWidgetTree& tree, GuiWidgetId parent,
                       const MarkdownBlock& block,
                       const MarkdownRenderConfig& config) {
    auto panel_id = createPanel(tree, parent);
    configurePanel(tree, panel_id, config.code_bg);
    setPadding(tree, panel_id, config.code_block_padding);
    setDebugName(tree, panel_id, "md_code_panel");
    InlineRenderCtx ctx{config};
    auto rt = buildRichText(block.inlines, ctx);
    auto label_id = createLabel(tree, panel_id);
    setLabelText(tree, label_id, rt);
    setDebugName(tree, label_id, "md_code_text");
  }

  struct ListItemParams {
    /// Item data to render.
    const MarkdownBlock& item;
    /// Zero-based index within the list.
    std::size_t index = 0;
    /// List type determines prefix style (bullet vs number).
    MarkdownBlockType list_type = MarkdownBlockType::BULLET_LIST;
    /// Style config.
    const MarkdownRenderConfig& config;
  };

  std::string listItemPrefix(const ListItemParams& p) {
    if (p.list_type == MarkdownBlockType::BULLET_LIST) {
      return "\xe2\x80\xa2";
    }
    return std::to_string(p.index + 1) + ".";
  }

  void setBulletText(GuiWidgetTree& tree, GuiWidgetId id,
                     const std::string& text) {
    auto* w = tree.findWidget(id);
    if (w == nullptr) {
      return;
    }
    auto* label = dynamic_cast<GuiLabel*>(w);
    if (label != nullptr) {
      label->text = text;
    }
  }

  void renderListItem(GuiWidgetTree& tree, GuiWidgetId parent,
                      const ListItemParams& p) {
    auto row_id = createPanel(tree, parent);
    setLayout(tree, row_id, FlexDirection::ROW, 0.0f);
    setDebugName(tree, row_id, "md_list_row");
    auto bullet_id = createLabel(tree, row_id);
    setBulletText(tree, bullet_id, listItemPrefix(p));
    InlineRenderCtx ctx{p.config};
    auto rt = buildRichText(p.item.inlines, ctx);
    auto text_id = createLabel(tree, row_id);
    setLabelText(tree, text_id, rt);
  }

  void renderList(GuiWidgetTree& tree, GuiWidgetId parent,
                  const MarkdownBlock& block,
                  const MarkdownRenderConfig& config) {
    auto list_id = createPanel(tree, parent);
    setLayout(tree, list_id, FlexDirection::COLUMN, 0.0f);
    setDebugName(tree, list_id, "md_list");
    for (std::size_t i = 0; i < block.children.size(); ++i) {
      ListItemParams p{block.children[i], i, block.type, config};
      renderListItem(tree, list_id, p);
    }
  }

  void renderBlockquote(GuiWidgetTree& tree, GuiWidgetId parent,
                        const MarkdownBlock& block,
                        const MarkdownRenderConfig& config) {
    auto panel_id = createPanel(tree, parent);
    configurePanel(tree, panel_id, config.blockquote_border_color);
    setDebugName(tree, panel_id, "md_blockquote");
    for (const auto& child : block.children) {
      InlineRenderCtx ctx{config};
      auto rt = buildRichText(child.inlines, ctx);
      auto label_id = createLabel(tree, panel_id);
      setLabelText(tree, label_id, rt);
    }
  }

  void renderHorizontalRule(GuiWidgetTree& tree, GuiWidgetId parent,
                            const MarkdownRenderConfig& config) {
    auto hr_id = createPanel(tree, parent);
    configurePanel(tree, hr_id, config.hr_color);
    setDebugName(tree, hr_id, "md_hr");
    auto* w = tree.findWidget(hr_id);
    if (w != nullptr) {
      w->tree_layout.height = config.hr_height;
    }
  }

  // ---------------------------------------------------------------------------
  // Table renderer
  // ---------------------------------------------------------------------------

  struct TableCellParams {
    /// Cell data containing inline spans.
    const MarkdownTableCell& cell;
    /// Column alignment for this cell.
    MarkdownColumnAlign align = MarkdownColumnAlign::LEFT;
    /// Style config.
    const MarkdownRenderConfig& config;
  };

  void configureCellLayout(GuiWidgetTree& tree, GuiWidgetId cell_id,
                           const TableCellParams& p) {
    setPadding(tree, cell_id, p.config.table_cell_padding);
    configureCellBorder(tree, cell_id, p.config);
    setFlexGrow(tree, cell_id, 1.0f);
    setWidthConstraints(tree, cell_id, p.config.table_min_column_width,
                        p.config.table_max_column_width);
    if (p.align == MarkdownColumnAlign::RIGHT) {
      auto* w = tree.findWidget(cell_id);
      if (w != nullptr) {
        w->tree_layout.justify_content = Align::END;
      }
    }
  }

  void renderTableCell(GuiWidgetTree& tree, GuiWidgetId row_id,
                       const TableCellParams& p) {
    auto cell_id = createPanel(tree, row_id);
    configureCellLayout(tree, cell_id, p);
    InlineRenderCtx ctx{p.config};
    auto rt = buildRichText(p.cell.inlines, ctx);
    auto label_id = createLabel(tree, cell_id);
    setLabelText(tree, label_id, rt);
    auto* lw = tree.findWidget(label_id);
    if (lw != nullptr) {
      auto* label = dynamic_cast<GuiLabel*>(lw);
      if (label != nullptr) {
        label->align = mapAlignment(p.align);
      }
    }
    setDebugName(tree, cell_id, "md_table_cell");
  }

  void renderTableHeader(GuiWidgetTree& tree, GuiWidgetId body_id,
                         const MarkdownTableData& data,
                         const MarkdownRenderConfig& config) {
    auto row_id = createPanel(tree, body_id);
    configurePanel(tree, row_id, config.table_header_bg);
    setLayout(tree, row_id, FlexDirection::ROW, 0.0f);
    setDebugName(tree, row_id, "md_table_header");
    for (std::size_t c = 0; c < data.header.cells.size(); ++c) {
      auto col_align =
          c < data.columns.size() ? data.columns[c] : MarkdownColumnAlign::LEFT;
      TableCellParams cp{data.header.cells[c], col_align, config};
      renderTableCell(tree, row_id, cp);
    }
  }

  void renderTableRows(GuiWidgetTree& tree, GuiWidgetId body_id,
                       const MarkdownTableData& data,
                       const MarkdownRenderConfig& config) {
    for (std::size_t r = 0; r < data.rows.size(); ++r) {
      auto bg =
          (r % 2 == 0) ? config.table_even_row_bg : config.table_odd_row_bg;
      auto row_id = createPanel(tree, body_id);
      configurePanel(tree, row_id, bg);
      setLayout(tree, row_id, FlexDirection::ROW, 0.0f);
      setDebugName(tree, row_id, "md_table_row");
      for (std::size_t c = 0; c < data.rows[r].cells.size(); ++c) {
        auto col_align = c < data.columns.size() ? data.columns[c]
                                                 : MarkdownColumnAlign::LEFT;
        TableCellParams cp{data.rows[r].cells[c], col_align, config};
        renderTableCell(tree, row_id, cp);
      }
    }
  }

  void renderTable(GuiWidgetTree& tree, GuiWidgetId parent,
                   const MarkdownBlock& block,
                   const MarkdownRenderConfig& config) {
    if (!block.table.has_value()) {
      return;
    }
    const auto& data = block.table.value();
    auto scroll_id = createScroll(tree, parent);
    auto* sw = tree.findWidget(scroll_id);
    if (sw != nullptr) {
      sw->tree_layout.scroll_x = true;
    }
    setDebugName(tree, scroll_id, "md_table_scroll");
    auto body_id = createPanel(tree, scroll_id);
    setLayout(tree, body_id, FlexDirection::COLUMN, 0.0f);
    setDebugName(tree, body_id, "md_table_body");
    renderTableHeader(tree, body_id, data, config);
    renderTableRows(tree, body_id, data, config);
  }

  // ---------------------------------------------------------------------------
  // Block dispatch
  // ---------------------------------------------------------------------------

  void renderBlock(GuiWidgetTree& tree, GuiWidgetId parent,
                   const MarkdownBlock& block,
                   const MarkdownRenderConfig& config) {
    switch (block.type) {
      case MarkdownBlockType::PARAGRAPH:
        renderParagraph(tree, parent, block, config);
        break;
      case MarkdownBlockType::HEADING:
        renderHeading(tree, parent, block, config);
        break;
      case MarkdownBlockType::CODE_BLOCK:
        renderCodeBlock(tree, parent, block, config);
        break;
      case MarkdownBlockType::BULLET_LIST:
      case MarkdownBlockType::ORDERED_LIST:
        renderList(tree, parent, block, config);
        break;
      case MarkdownBlockType::LIST_ITEM:
        break;
      case MarkdownBlockType::BLOCKQUOTE:
        renderBlockquote(tree, parent, block, config);
        break;
      case MarkdownBlockType::HORIZONTAL_RULE:
        renderHorizontalRule(tree, parent, config);
        break;
      case MarkdownBlockType::TABLE:
        renderTable(tree, parent, block, config);
        break;
    }
  }

  void destroyChildren(GuiWidgetTree& tree, GuiWidgetId parent) {
    auto* w = tree.findWidget(parent);
    if (w == nullptr) {
      return;
    }
    auto children_copy = w->children;
    for (auto child : children_copy) {
      tree.destroyWidget(child);
    }
  }

  void destroyLastChild(GuiWidgetTree& tree, GuiWidgetId parent) {
    auto* w = tree.findWidget(parent);
    if (w == nullptr || w->children.empty()) {
      return;
    }
    tree.destroyWidget(w->children.back());
  }

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void MarkdownRenderer::render(GuiWidgetTree& tree,
                              const MarkdownRenderParams& params,
                              std::span<const MarkdownBlock> blocks) {
  if (params.parent_id == GUI_WIDGET_ID_INVALID) {
    return;
  }
  const auto& config = resolveConfig(params.config);
  if (params.previous_complete_count == 0) {
    destroyChildren(tree, params.parent_id);
    for (const auto& block : blocks) {
      renderBlock(tree, params.parent_id, block, config);
    }
    return;
  }
  destroyLastChild(tree, params.parent_id);
  auto start = params.previous_complete_count > 0
                   ? params.previous_complete_count - 1
                   : std::size_t{0};
  for (std::size_t i = start; i < blocks.size(); ++i) {
    renderBlock(tree, params.parent_id, blocks[i], config);
  }
}

void MarkdownRenderer::clear(GuiWidgetTree& tree, GuiWidgetId parent_id) {
  destroyChildren(tree, parent_id);
}

}  // namespace eng::gui
