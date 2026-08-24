#pragma once

/// @file markdown-render-config.h
/// @brief Tunable style and layout values for Markdown widget rendering.
/// @par Threading Main thread only.

#include "gui-color.h"

namespace eng::gui {

/// Configuration for rendering Markdown blocks as GUI widgets.
/// Passed as const& to MarkdownRenderer. Defaults produce a dark-theme
/// appearance suitable for AI chat panels.
/// @thread_safety Main thread only.
struct MarkdownRenderConfig {
  /// Base font size in pixels for body text.
  float base_font_size = 14.0f;
  /// Heading 1 font-size multiplier relative to base.
  float h1_scale = 1.75f;
  /// Heading 2 font-size multiplier relative to base.
  float h2_scale = 1.5f;
  /// Heading 3 font-size multiplier relative to base.
  float h3_scale = 1.25f;
  /// Inner padding for code block panels in pixels.
  float code_block_padding = 8.0f;
  /// Horizontal indent per list nesting level in pixels.
  float list_indent = 20.0f;
  /// Horizontal indent for blockquote content in pixels.
  float blockquote_indent = 16.0f;
  /// Width of the left border accent on blockquotes in pixels.
  float blockquote_border_width = 3.0f;
  /// Vertical gap between consecutive blocks in pixels.
  float block_spacing = 6.0f;
  /// Height of horizontal rule lines in pixels.
  float hr_height = 1.0f;
  /// Default text colour.
  GuiColor text_color{220, 220, 220, 255};
  /// Code block background fill colour.
  GuiColor code_bg{45, 45, 45, 255};
  /// Text colour inside code blocks and inline code spans.
  GuiColor code_text_color{200, 200, 200, 255};
  /// Left border colour for blockquotes.
  GuiColor blockquote_border_color{100, 100, 180, 255};
  /// Text colour inside blockquotes.
  GuiColor blockquote_text_color{180, 180, 200, 255};
  /// Text colour for hyperlinks.
  GuiColor link_color{100, 160, 255, 255};
  /// Fill colour for horizontal rule lines.
  GuiColor hr_color{80, 80, 80, 255};
  /// Background colour for table header row.
  GuiColor table_header_bg{50, 55, 65, 255};
  /// Background colour for even-numbered data rows (0-indexed).
  GuiColor table_even_row_bg{35, 38, 45, 255};
  /// Background colour for odd-numbered data rows.
  GuiColor table_odd_row_bg{40, 43, 52, 255};
  /// Border colour between table cells.
  GuiColor table_border_color{65, 70, 80, 255};
  /// Inner padding for each table cell in pixels.
  float table_cell_padding = 6.0f;
  /// Border width between table cells in pixels.
  float table_border_width = 1.0f;
  /// Minimum column width in pixels.
  float table_min_column_width = 40.0f;
  /// Maximum column width in pixels.
  float table_max_column_width = 300.0f;
};

}  // namespace eng::gui
