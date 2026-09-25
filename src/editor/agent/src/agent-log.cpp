#include "agent-log.h"

#include "agent-call.h"

#include <algorithm>
#include <editor/agent/agent-names.h>
#include <string>

namespace eng::editor {

namespace {

  using nlohmann::json;

  /// Most lines one call answers with.
  constexpr size_t MAX_LOG_LINES = 500;
  /// Lines a call answers with when it does not say.
  constexpr size_t DEFAULT_LOG_LINES = 100;

  /// What a call asks of the log.
  struct LogQuery {
    /// The first sequence number wanted.
    uint64_t since = 0;
    /// The least serious level wanted.
    LogLevel level = LogLevel::INFO;
    /// The one subsystem wanted, or empty for all.
    std::string subsystem;
    /// Most lines to answer with.
    size_t limit = DEFAULT_LOG_LINES;
  };

  /// Whether @p entry is one @p query asks for.
  bool wanted(const LogQuery& query, const EditorLogEntry& entry) {
    return entry.sequence >= query.since && entry.level >= query.level &&
           (query.subsystem.empty() || entry.subsystem == query.subsystem);
  }

  json entryJson(const EditorLogEntry& entry) {
    return {{"seq", entry.sequence},
            {"level", agentLogLevelName(entry.level)},
            {"subsystem", entry.subsystem},
            {"message", entry.message}};
  }

  /// The lines @p query asks of @p log, and whether more were left out.
  json answer(const EditorLogState& log, const LogQuery& query) {
    json entries = json::array();
    bool more = false;
    for (const EditorLogEntry& entry : log.entries) {
      if (!wanted(query, entry)) {
        continue;
      }
      if (entries.size() == query.limit) {
        more = true;
        break;
      }
      entries.push_back(entryJson(entry));
    }
    return {{"entries", entries}, {"more", more}, {"next", log.next}};
  }

}  // namespace

AgentResult runAgentGetLog(EditorShellState& state, const json& params) {
  LogQuery query;
  query.since = agentIndexParam(params, "since").value_or(0);
  query.subsystem = agentStringParam(params, "subsystem").value_or("");
  query.limit = std::clamp<size_t>(
      agentIndexParam(params, "limit").value_or(DEFAULT_LOG_LINES), 1,
      MAX_LOG_LINES);
  const std::string level = agentStringParam(params, "level").value_or("info");
  const std::optional<LogLevel> parsed = agentLogLevelFromName(level);
  if (!parsed) {
    return agentFailure(AgentStatus::BAD_PARAMS,
                        "level is debug, info, warn or error");
  }
  query.level = *parsed;
  return agentOk(answer(state.log, query).dump(2));
}

}  // namespace eng::editor
