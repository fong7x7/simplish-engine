#include <catch2/catch_test_macros.hpp>
#include <editor/shell/editor-log-book.h>
#include <engine/core/logger.h>
#include <string>

using namespace eng;
using namespace eng::editor;

TEST_CASE("the log book catches what is logged, numbered in order") {
  EditorLogBook book;
  EditorLogState state;
  LOG_WARN("test", "first");
  LOG_ERROR("test", "second");

  book.drainInto(state);

  REQUIRE(state.entries.size() >= 2);
  const EditorLogEntry& last = state.entries.back();
  CHECK(last.message == "second");
  CHECK(last.level == LogLevel::ERROR);
  CHECK(last.sequence + 1 == state.next);
}

TEST_CASE("the log book keeps only the last lines") {
  EditorLogBook book;
  EditorLogState state;
  for (size_t i = 0; i < EDITOR_LOG_LINES + 20; ++i) {
    LOG_WARN("test", "line " + std::to_string(i));
  }

  book.drainInto(state);

  CHECK(state.entries.size() == EDITOR_LOG_LINES);
  CHECK(state.entries.back().message ==
        "line " + std::to_string(EDITOR_LOG_LINES + 19));
}
