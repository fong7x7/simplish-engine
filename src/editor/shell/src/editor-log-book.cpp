#include <editor/shell/editor-log-book.h>
#include <iterator>
#include <string>
#include <utility>

namespace eng::editor {

namespace {

  /// Drop the oldest of @p lines until at most `EDITOR_LOG_LINES` remain.
  void keepLast(std::vector<EditorLogEntry>& lines) {
    if (lines.size() > EDITOR_LOG_LINES) {
      lines.erase(lines.begin(),
                  lines.end() - static_cast<std::ptrdiff_t>(EDITOR_LOG_LINES));
    }
  }

}  // namespace

EditorLogBook::EditorLogBook()
  : sink_(
        Logger::registerSink([this](LogLevel level, std::string_view subsystem,
                                    std::string_view message) {
          record(level, subsystem, message);
        })) {}

EditorLogBook::~EditorLogBook() {
  Logger::unregisterSink(sink_);
}

void EditorLogBook::record(LogLevel level, std::string_view subsystem,
                           std::string_view message) {
  const std::scoped_lock lock(mutex_);
  caught_.push_back({0, level, std::string(subsystem), std::string(message)});
  keepLast(caught_);
}

void EditorLogBook::drainInto(EditorLogState& state) {
  std::vector<EditorLogEntry> lines;
  {
    const std::scoped_lock lock(mutex_);
    lines.swap(caught_);
  }
  for (EditorLogEntry& line : lines) {
    line.sequence = state.next++;
    state.entries.push_back(std::move(line));
  }
  keepLast(state.entries);
}

}  // namespace eng::editor
