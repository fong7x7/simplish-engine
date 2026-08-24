#include <algorithm>
#include <engine/core/audit/audit-query.h>

namespace eng {

namespace {

  // -----------------------------------------------------------------------
  // Named algorithm: isValidRegexPattern
  //
  // Purpose : Validate that a regex pattern has balanced brackets and
  //           parentheses, rejecting obviously malformed patterns before
  //           they reach the regex engine.
  // Input   : pattern — a regex pattern string (non-owning view).
  // Output  : true if brackets/parens are balanced and no trailing escape.
  // Side effects : None (pure function).
  // -----------------------------------------------------------------------
  // NOLINTNEXTLINE(readability-function-size) -- named algorithm, pure function
  bool isValidRegexPattern(std::string_view pattern) {
    int paren_depth = 0;
    int bracket_depth = 0;
    bool escaped = false;
    for (char ch : pattern) {
      if (escaped) {
        escaped = false;
        continue;
      }
      if (ch == '\\') {
        escaped = true;
        continue;
      }
      if (bracket_depth > 0) {
        if (ch == ']') {
          --bracket_depth;
        }
        continue;
      }
      if (ch == '[') {
        ++bracket_depth;
      } else if (ch == '(') {
        ++paren_depth;
      } else if (ch == ')') {
        --paren_depth;
      }
      if (paren_depth < 0) {
        return false;
      }
    }
    return paren_depth == 0 && bracket_depth == 0 && !escaped;
  }

}  // namespace

AuditQueryContext createQueryContext(const AuditMergeContext& merge,
                                     const AuditSchemaRegistry& schema,
                                     const AuditErrorRegistry& errors) {
  AuditQueryContext ctx{};
  ctx.merge_ctx = &merge;
  ctx.schema_registry = &schema;
  ctx.error_registry = &errors;
  return ctx;
}

AuditQueryResult queryEvents(const AuditQueryContext& /*ctx*/,
                             const AuditFilter& filter) {
  if (filter.text_pattern.has_value() &&
      !isValidRegexPattern(*filter.text_pattern)) {
    return AuditQueryError::INVALID_REGEX;
  }
  return std::vector<AuditEventRecord>{};
}

AuditStats getAuditStats(const AuditQueryContext& /*ctx*/) {
  return AuditStats{0, 0.0f, 0.0f, 0, 0, {}};
}

std::vector<AuditActorStats> getTopActors(const AuditQueryContext& /*ctx*/,
                                          uint32_t /*top_n*/,
                                          uint64_t /*start_ns*/,
                                          uint64_t /*end_ns*/) {
  return {};
}

std::vector<AuditErrorSummary>
getErrorSummary(const AuditQueryContext& /*ctx*/) {
  return {};
}

void addBookmark(AuditQueryContext& ctx, std::string_view name,
                 uint64_t timestamp_ns) {
  auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
      ctx.bookmarks.begin(), ctx.bookmarks.end(),
      [&](const AuditBookmark& b) { return b.name == name; });
  if (it != ctx.bookmarks.end()) {
    it->timestamp_ns = timestamp_ns;
  } else {
    ctx.bookmarks.push_back({std::string(name), timestamp_ns});
  }
}

std::span<const AuditBookmark> getBookmarks(const AuditQueryContext& ctx) {
  return ctx.bookmarks;
}

bool removeBookmark(AuditQueryContext& ctx, std::string_view name) {
  auto it = std::find_if(  // NOLINT(modernize-use-ranges,llvm-use-ranges)
      ctx.bookmarks.begin(), ctx.bookmarks.end(),
      [&](const AuditBookmark& b) { return b.name == name; });
  if (it == ctx.bookmarks.end()) {
    return false;
  }
  ctx.bookmarks.erase(it);
  return true;
}

uint64_t subscribeEvents(AuditQueryContext& ctx, AuditEventCallback callback) {
  const uint64_t handle = ctx.next_sub_handle++;
  ctx.subscriptions.push_back({handle, callback});
  return handle;
}

bool unsubscribeEvents(AuditQueryContext& ctx, uint64_t handle) {
  auto& subs = ctx.subscriptions;
  // NOLINTNEXTLINE(modernize-use-ranges,llvm-use-ranges)
  auto it = std::find_if(subs.begin(), subs.end(), [handle](const auto& s) {
    return s.handle == handle;
  });
  if (it == subs.end()) {
    return false;
  }
  subs.erase(it);
  return true;
}

}  // namespace eng
