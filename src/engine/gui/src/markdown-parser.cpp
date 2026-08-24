/// @file markdown-parser.cpp
/// @brief Stateless Markdown parser — string to block AST.

#include <algorithm>
#include <engine/gui/markdown-column-align.h>
#include <engine/gui/markdown-parser.h>
#include <engine/gui/markdown-table-data.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace eng::gui {
namespace {

  // ---------------------------------------------------------------------------
  // Line utilities
  // ---------------------------------------------------------------------------

  std::vector<std::string_view> splitLines(std::string_view input) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= input.size()) {
      auto pos = input.find('\n', start);
      if (pos == std::string_view::npos) {
        auto line = input.substr(start);
        if (!line.empty() && line.back() == '\r') {
          line.remove_suffix(1);
        }
        lines.push_back(line);
        break;
      }
      auto line = input.substr(start, pos - start);
      if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
      }
      lines.push_back(line);
      start = pos + 1;
    }
    return lines;
  }

  std::string_view trimWhitespace(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
      s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
      s.remove_suffix(1);
    }
    return s;
  }

  bool isBlankLine(std::string_view line) {
    return trimWhitespace(line).empty();
  }

  // ---------------------------------------------------------------------------
  // Inline parser (Pass 2)
  // ---------------------------------------------------------------------------

  MarkdownInline makeTextInline(std::string text) {
    return MarkdownInline{MarkdownInlineType::TEXT, std::move(text), {}};
  }

  void flushAccum(std::vector<MarkdownInline>& out, std::string& accum) {
    if (!accum.empty()) {
      out.push_back(makeTextInline(std::move(accum)));
      accum.clear();
    }
  }

  struct LinkSpan {
    /// Position of the closing bracket.
    std::size_t close_bracket = 0;
    /// Position of the closing parenthesis.
    std::size_t close_paren = 0;
  };

  std::optional<LinkSpan> findLinkSpan(std::string_view src, std::size_t i) {
    auto close_bracket = src.find(']', i + 1);
    if (close_bracket == std::string_view::npos) {
      return std::nullopt;
    }
    if (close_bracket + 1 >= src.size() || src[close_bracket + 1] != '(') {
      return std::nullopt;
    }
    auto close_paren = src.find(')', close_bracket + 2);
    if (close_paren == std::string_view::npos) {
      return std::nullopt;
    }
    return LinkSpan{close_bracket, close_paren};
  }

  bool tryParseLink(std::string_view src, std::size_t& i,
                    std::vector<MarkdownInline>& out, std::string& accum) {
    auto span = findLinkSpan(src, i);
    if (!span.has_value()) {
      return false;
    }
    flushAccum(out, accum);
    auto text = src.substr(i + 1, span->close_bracket - i - 1);
    auto url = src.substr(span->close_bracket + 2,
                          span->close_paren - span->close_bracket - 2);
    out.push_back(
        {MarkdownInlineType::LINK, std::string(text), std::string(url)});
    i = span->close_paren + 1;
    return true;
  }

  bool tryParseInlineCode(std::string_view src, std::size_t& i,
                          std::vector<MarkdownInline>& out,
                          std::string& accum) {
    auto close = src.find('`', i + 1);
    if (close == std::string_view::npos) {
      return false;
    }
    flushAccum(out, accum);
    auto code = src.substr(i + 1, close - i - 1);
    out.push_back({MarkdownInlineType::INLINE_CODE, std::string(code), {}});
    i = close + 1;
    return true;
  }

  struct EmphResult {
    /// Number of delimiter chars consumed (1, 2, or 3).
    std::size_t delim_len = 0;
    /// The inline type produced.
    MarkdownInlineType type = MarkdownInlineType::TEXT;
  };

  EmphResult classifyEmph(std::string_view src, std::size_t i) {
    std::size_t run = 0;
    while (i + run < src.size() && src[i + run] == '*') {
      ++run;
    }
    if (run >= 3) {
      return {3, MarkdownInlineType::BOLD_ITALIC};
    }
    if (run == 2) {
      return {2, MarkdownInlineType::BOLD};
    }
    return {1, MarkdownInlineType::ITALIC};
  }

  bool tryParseEmph(std::string_view src, std::size_t& i,
                    std::vector<MarkdownInline>& out, std::string& accum) {
    auto emph = classifyEmph(src, i);
    std::string_view delim = src.substr(i, emph.delim_len);
    auto close = src.find(delim, i + emph.delim_len);
    if (close == std::string_view::npos) {
      return false;
    }
    flushAccum(out, accum);
    auto text = src.substr(i + emph.delim_len, close - i - emph.delim_len);
    out.push_back({emph.type, std::string(text), {}});
    i = close + emph.delim_len;
    return true;
  }

  bool tryParseSpecial(std::string_view text, std::size_t& i,
                       std::vector<MarkdownInline>& out, std::string& accum) {
    char ch = text[i];
    if (ch == '[' && tryParseLink(text, i, out, accum)) {
      return true;
    }
    if (ch == '`' && tryParseInlineCode(text, i, out, accum)) {
      return true;
    }
    if (ch == '*' && tryParseEmph(text, i, out, accum)) {
      return true;
    }
    return false;
  }

  std::vector<MarkdownInline> parseInlines(std::string_view text) {
    std::vector<MarkdownInline> result;
    std::string accum;
    std::size_t i = 0;
    while (i < text.size()) {
      if (tryParseSpecial(text, i, result, accum)) {
        continue;
      }
      accum += text[i];
      ++i;
    }
    flushAccum(result, accum);
    return result;
  }

  // ---------------------------------------------------------------------------
  // Table detection & parsing
  // ---------------------------------------------------------------------------

  bool containsPipe(std::string_view line) {
    return line.find('|') != std::string_view::npos;
  }

  std::string_view stripOuterPipes(std::string_view line) {
    auto trimmed = trimWhitespace(line);
    if (!trimmed.empty() && trimmed.front() == '|') {
      trimmed.remove_prefix(1);
    }
    if (!trimmed.empty() && trimmed.back() == '|') {
      trimmed.remove_suffix(1);
    }
    return trimmed;
  }

  std::vector<std::string_view> splitTableCells(std::string_view line) {
    std::vector<std::string_view> cells;
    auto trimmed = stripOuterPipes(line);
    std::size_t start = 0;
    while (start <= trimmed.size()) {
      auto pos = trimmed.find('|', start);
      if (pos == std::string_view::npos) {
        cells.push_back(trimWhitespace(trimmed.substr(start)));
        break;
      }
      cells.push_back(trimWhitespace(trimmed.substr(start, pos - start)));
      start = pos + 1;
    }
    return cells;
  }

  MarkdownColumnAlign parseOneAlignment(std::string_view cell) {
    auto s = trimWhitespace(cell);
    bool left_colon = !s.empty() && s.front() == ':';
    bool right_colon = !s.empty() && s.back() == ':';
    if (left_colon && right_colon) {
      return MarkdownColumnAlign::CENTER;
    }
    if (right_colon) {
      return MarkdownColumnAlign::RIGHT;
    }
    return MarkdownColumnAlign::LEFT;
  }

  bool isSeparatorCell(std::string_view cell) {
    auto s = trimWhitespace(cell);
    if (s.empty()) {
      return false;
    }
    if (s.front() == ':') {
      s.remove_prefix(1);
    }
    if (!s.empty() && s.back() == ':') {
      s.remove_suffix(1);
    }
    if (s.size() < 3) {
      return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) { return c == '-'; });
  }

  bool isTableSeparator(std::string_view line) {
    if (!containsPipe(line)) {
      return false;
    }
    auto cells = splitTableCells(line);
    if (cells.empty()) {
      return false;
    }
    return std::all_of(cells.begin(), cells.end(), isSeparatorCell);
  }

  std::vector<MarkdownColumnAlign> parseAlignments(std::string_view sep_line) {
    auto cells = splitTableCells(sep_line);
    std::vector<MarkdownColumnAlign> aligns;
    aligns.reserve(cells.size());
    for (auto cell : cells) {
      aligns.push_back(parseOneAlignment(cell));
    }
    return aligns;
  }

  MarkdownTableRow parseTableRow(std::string_view line, std::size_t col_count) {
    auto raw_cells = splitTableCells(line);
    MarkdownTableRow row;
    row.cells.reserve(col_count);
    for (std::size_t c = 0; c < col_count; ++c) {
      MarkdownTableCell cell;
      if (c < raw_cells.size()) {
        cell.inlines = parseInlines(raw_cells[c]);
      }
      row.cells.push_back(std::move(cell));
    }
    return row;
  }

  // ---------------------------------------------------------------------------
  // Block-level line classification
  // ---------------------------------------------------------------------------

  enum class LineKind : uint8_t {
    BLANK,
    HEADING,
    CODE_FENCE,
    BULLET,
    ORDERED,
    BLOCKQUOTE,
    HR,
    TEXT,
  };

  bool startsWithBullet(std::string_view line) {
    auto s = trimWhitespace(line);
    return s.size() >= 2 && (s[0] == '-' || s[0] == '*') && s[1] == ' ';
  }

  bool startsWithOrdered(std::string_view line) {
    auto s = trimWhitespace(line);
    std::size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      ++i;
    }
    return i > 0 && i < s.size() && s[i] == '.' && i + 1 < s.size() &&
           s[i + 1] == ' ';
  }

  bool isHorizontalRule(std::string_view line) {
    auto s = trimWhitespace(line);
    if (s.size() < 3) {
      return false;
    }
    char ch = s[0];
    if (ch != '-' && ch != '*' && ch != '_') {
      return false;
    }
    return std::all_of(s.begin(), s.end(), [ch](char c) { return c == ch; });
  }

  bool isCodeFence(std::string_view line) {
    auto s = trimWhitespace(line);
    return s.size() >= 3 && s[0] == '`' && s[1] == '`' && s[2] == '`';
  }

  bool isHeading(std::string_view line) {
    auto s = trimWhitespace(line);
    if (s.empty() || s[0] != '#') {
      return false;
    }
    std::size_t level = 0;
    while (level < s.size() && s[level] == '#') {
      ++level;
    }
    return level <= 3 && level < s.size() && s[level] == ' ';
  }

  bool isBlockquote(std::string_view line) {
    auto s = trimWhitespace(line);
    return !s.empty() && s[0] == '>';
  }

  LineKind classifyStructuralLine(std::string_view line) {
    if (isCodeFence(line)) {
      return LineKind::CODE_FENCE;
    }
    if (isHeading(line)) {
      return LineKind::HEADING;
    }
    if (isHorizontalRule(line)) {
      return LineKind::HR;
    }
    return LineKind::TEXT;
  }

  LineKind classifyContentLine(std::string_view line) {
    if (startsWithBullet(line)) {
      return LineKind::BULLET;
    }
    if (startsWithOrdered(line)) {
      return LineKind::ORDERED;
    }
    if (isBlockquote(line)) {
      return LineKind::BLOCKQUOTE;
    }
    return LineKind::TEXT;
  }

  LineKind classifyLine(std::string_view line) {
    if (isBlankLine(line)) {
      return LineKind::BLANK;
    }
    auto structural = classifyStructuralLine(line);
    if (structural != LineKind::TEXT) {
      return structural;
    }
    return classifyContentLine(line);
  }

  // ---------------------------------------------------------------------------
  // Block builders
  // ---------------------------------------------------------------------------

  uint8_t parseHeadingLevel(std::string_view line) {
    auto s = trimWhitespace(line);
    uint8_t level = 0;
    while (level < s.size() && s[level] == '#') {
      ++level;
    }
    return level;
  }

  std::string_view stripHeadingPrefix(std::string_view line) {
    auto s = trimWhitespace(line);
    while (!s.empty() && s[0] == '#') {
      s.remove_prefix(1);
    }
    return trimWhitespace(s);
  }

  std::string_view stripBulletPrefix(std::string_view line) {
    auto s = trimWhitespace(line);
    if (s.size() >= 2 && (s[0] == '-' || s[0] == '*') && s[1] == ' ') {
      return s.substr(2);
    }
    return s;
  }

  std::string_view stripOrderedPrefix(std::string_view line) {
    auto s = trimWhitespace(line);
    std::size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      ++i;
    }
    if (i < s.size() && s[i] == '.' && i + 1 < s.size() && s[i + 1] == ' ') {
      return s.substr(i + 2);
    }
    return s;
  }

  std::string_view stripBlockquotePrefix(std::string_view line) {
    auto s = trimWhitespace(line);
    if (!s.empty() && s[0] == '>') {
      s.remove_prefix(1);
      if (!s.empty() && s[0] == ' ') {
        s.remove_prefix(1);
      }
    }
    return s;
  }

  std::string extractCodeLanguage(std::string_view fence_line) {
    auto s = trimWhitespace(fence_line);
    if (s.size() > 3) {
      return std::string(trimWhitespace(s.substr(3)));
    }
    return {};
  }

  MarkdownBlock buildHeading(std::string_view line) {
    MarkdownBlock block;
    block.type = MarkdownBlockType::HEADING;
    block.heading_level = parseHeadingLevel(line);
    block.inlines = parseInlines(stripHeadingPrefix(line));
    return block;
  }

  MarkdownBlock buildParagraph(std::string_view text) {
    MarkdownBlock block;
    block.type = MarkdownBlockType::PARAGRAPH;
    block.inlines = parseInlines(text);
    return block;
  }

  MarkdownBlock buildHorizontalRule() {
    MarkdownBlock block;
    block.type = MarkdownBlockType::HORIZONTAL_RULE;
    return block;
  }

  MarkdownBlock buildListItem(std::string_view text) {
    MarkdownBlock block;
    block.type = MarkdownBlockType::LIST_ITEM;
    block.inlines = parseInlines(text);
    return block;
  }

  std::vector<MarkdownTableRow>
  collectDataRows(const std::vector<std::string_view>& lines, std::size_t start,
                  std::size_t end, std::size_t col_count) {
    std::vector<MarkdownTableRow> rows;
    for (std::size_t r = start; r < end; ++r) {
      if (isBlankLine(lines[r]) || !containsPipe(lines[r])) {
        break;
      }
      rows.push_back(parseTableRow(lines[r], col_count));
    }
    return rows;
  }

  MarkdownBlock buildTable(const std::vector<std::string_view>& lines,
                           std::size_t header_idx, std::size_t sep_idx,
                           std::size_t end_idx) {
    auto aligns = parseAlignments(lines[sep_idx]);
    auto col_count = aligns.size();
    MarkdownTableData data;
    data.columns = std::move(aligns);
    data.header = parseTableRow(lines[header_idx], col_count);
    data.rows = collectDataRows(lines, sep_idx + 1, end_idx, col_count);
    MarkdownBlock block;
    block.type = MarkdownBlockType::TABLE;
    block.table = std::move(data);
    return block;
  }

  // ---------------------------------------------------------------------------
  // Block scanner (Pass 1)
  // ---------------------------------------------------------------------------

  struct ScanState {
    /// Source lines.
    const std::vector<std::string_view>& lines;
    /// Current scan position.
    std::size_t pos = 0;
    /// Output blocks.
    std::vector<MarkdownBlock>& out;
  };

  std::string accumulateCodeContent(ScanState& s) {
    std::string content;
    while (s.pos < s.lines.size() && !isCodeFence(s.lines[s.pos])) {
      if (!content.empty()) {
        content += '\n';
      }
      content += s.lines[s.pos];
      ++s.pos;
    }
    if (s.pos < s.lines.size()) {
      ++s.pos;
    }
    return content;
  }

  void scanCodeBlock(ScanState& s) {
    auto lang = extractCodeLanguage(s.lines[s.pos]);
    ++s.pos;
    auto content = accumulateCodeContent(s);
    MarkdownBlock block;
    block.type = MarkdownBlockType::CODE_BLOCK;
    block.language = std::move(lang);
    block.inlines.push_back(makeTextInline(std::move(content)));
    s.out.push_back(std::move(block));
  }

  void scanListBlock(ScanState& s, MarkdownBlockType list_type) {
    MarkdownBlock list_block;
    list_block.type = list_type;
    while (s.pos < s.lines.size()) {
      bool is_item = (list_type == MarkdownBlockType::BULLET_LIST)
                         ? startsWithBullet(s.lines[s.pos])
                         : startsWithOrdered(s.lines[s.pos]);
      if (!is_item) {
        break;
      }
      auto text = (list_type == MarkdownBlockType::BULLET_LIST)
                      ? stripBulletPrefix(s.lines[s.pos])
                      : stripOrderedPrefix(s.lines[s.pos]);
      list_block.children.push_back(buildListItem(text));
      ++s.pos;
    }
    s.out.push_back(std::move(list_block));
  }

  std::string accumulateQuoteText(ScanState& s) {
    std::string combined;
    while (s.pos < s.lines.size() && isBlockquote(s.lines[s.pos])) {
      if (!combined.empty()) {
        combined += '\n';
      }
      combined += stripBlockquotePrefix(s.lines[s.pos]);
      ++s.pos;
    }
    return combined;
  }

  void scanBlockquote(ScanState& s) {
    auto combined = accumulateQuoteText(s);
    MarkdownBlock block;
    block.type = MarkdownBlockType::BLOCKQUOTE;
    block.children.push_back(buildParagraph(combined));
    s.out.push_back(std::move(block));
  }

  bool isTableStart(const ScanState& s) {
    if (s.pos + 1 >= s.lines.size()) {
      return false;
    }
    return containsPipe(s.lines[s.pos]) && isTableSeparator(s.lines[s.pos + 1]);
  }

  std::size_t findTableEnd(const ScanState& s, std::size_t start) {
    auto end = start;
    while (end < s.lines.size() && containsPipe(s.lines[end]) &&
           !isBlankLine(s.lines[end])) {
      ++end;
    }
    return end;
  }

  bool tryScanTable(ScanState& s) {
    if (!isTableStart(s)) {
      return false;
    }
    auto header_idx = s.pos;
    auto sep_idx = s.pos + 1;
    auto end_idx = findTableEnd(s, sep_idx + 1);
    s.out.push_back(buildTable(s.lines, header_idx, sep_idx, end_idx));
    s.pos = end_idx;
    return true;
  }

  bool isParagraphBreak(const ScanState& s) {
    if (classifyLine(s.lines[s.pos]) != LineKind::TEXT) {
      return true;
    }
    return containsPipe(s.lines[s.pos]) && s.pos + 1 < s.lines.size() &&
           isTableSeparator(s.lines[s.pos + 1]);
  }

  std::string accumulateParagraphText(ScanState& s) {
    std::string combined;
    while (s.pos < s.lines.size() && !isParagraphBreak(s)) {
      if (!combined.empty()) {
        combined += ' ';
      }
      combined += trimWhitespace(s.lines[s.pos]);
      ++s.pos;
    }
    return combined;
  }

  void scanParagraph(ScanState& s) {
    auto combined = accumulateParagraphText(s);
    if (!combined.empty()) {
      s.out.push_back(buildParagraph(combined));
    }
  }

  bool tryScanSingleLine(ScanState& s, LineKind kind) {
    if (kind == LineKind::HEADING) {
      s.out.push_back(buildHeading(s.lines[s.pos]));
      ++s.pos;
      return true;
    }
    if (kind == LineKind::HR) {
      s.out.push_back(buildHorizontalRule());
      ++s.pos;
      return true;
    }
    return false;
  }

  bool tryScanList(ScanState& s, LineKind kind) {
    if (kind == LineKind::BULLET) {
      scanListBlock(s, MarkdownBlockType::BULLET_LIST);
      return true;
    }
    if (kind == LineKind::ORDERED) {
      scanListBlock(s, MarkdownBlockType::ORDERED_LIST);
      return true;
    }
    return false;
  }

  bool tryScanMultiLine(ScanState& s, LineKind kind) {
    if (kind == LineKind::CODE_FENCE) {
      scanCodeBlock(s);
      return true;
    }
    if (tryScanList(s, kind)) {
      return true;
    }
    if (kind == LineKind::BLOCKQUOTE) {
      scanBlockquote(s);
      return true;
    }
    return false;
  }

  void scanOneBlock(ScanState& s) {
    auto kind = classifyLine(s.lines[s.pos]);
    if (tryScanSingleLine(s, kind) || tryScanMultiLine(s, kind) ||
        tryScanTable(s)) {
      return;
    }
    scanParagraph(s);
  }

  void scanBlocks(ScanState& s) {
    while (s.pos < s.lines.size()) {
      if (isBlankLine(s.lines[s.pos])) {
        ++s.pos;
        continue;
      }
      scanOneBlock(s);
    }
  }

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<MarkdownBlock> MarkdownParser::parse(std::string_view input) {
  if (input.empty()) {
    return {};
  }
  auto lines = splitLines(input);
  std::vector<MarkdownBlock> blocks;
  ScanState state{lines, 0, blocks};
  scanBlocks(state);
  return blocks;
}

}  // namespace eng::gui
