#pragma once

/// @file editor-log-book.h
/// @brief Catches every line logged, for the editor to show agents.
/// @par Threading `record` is called by the logger from whichever thread
/// logs; `drainInto` is main-thread-only.

#include <editor/shell/editor-log-entry.h>
#include <editor/shell/editor-log-state.h>
#include <engine/core/logger.h>
#include <mutex>
#include <vector>

namespace eng::editor {

/// A logger sink, registered for as long as it lives, that keeps what is
/// logged until the editor moves it into its shell state. A lock, because
/// anything may log from any thread; nothing but that.
/// @thread_safety See the file header.
class EditorLogBook {
public:
  /// A book catching everything logged from now on.
  EditorLogBook();
  ~EditorLogBook();

  EditorLogBook(const EditorLogBook&) = delete;
  EditorLogBook& operator=(const EditorLogBook&) = delete;
  EditorLogBook(EditorLogBook&&) = delete;
  EditorLogBook& operator=(EditorLogBook&&) = delete;

  /// Move every line caught since the last call into @p state, numbering
  /// them and keeping only the last `EDITOR_LOG_LINES`.
  void drainInto(EditorLogState& state);

private:
  /// Keep one line; the logger's sink.
  void record(LogLevel level, std::string_view subsystem,
              std::string_view message);

  /// Guards `caught_`.
  std::mutex mutex_;
  /// Lines caught since the last drain, unnumbered; at most
  /// `EDITOR_LOG_LINES`, the oldest dropped first.
  std::vector<EditorLogEntry> caught_;
  /// The logger's handle on this sink.
  Logger::SinkHandle sink_ = 0;
};

}  // namespace eng::editor
