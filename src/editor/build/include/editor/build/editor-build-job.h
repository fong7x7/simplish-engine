#pragma once

/// @file editor-build-job.h
/// @brief A build, running its commands on a thread of its own.
/// @par Threading `start`, `status` and the destructor are main-thread-only;
/// the commands run on the job's own thread, which touches nothing but the
/// log file and the job's atomics.

#include <atomic>
#include <cstdint>
#include <editor/build/editor-build-command.h>
#include <editor/build/editor-build-status.h>
#include <filesystem>
#include <memory>
#include <thread>
#include <vector>

namespace eng::editor {

/// Runs a build's commands one after another, in the background, so a
/// compile that takes minutes does not stop the editor drawing. Stops at
/// the first command that fails. Everything the commands print goes to a
/// log file, which is what the editor — and an agent — reads to see why.
///
/// One build at a time: `start` refuses while one runs. The destructor
/// never waits on one: quitting the editor mid-deploy must not hang it for
/// the minutes an engine build takes. A running build's thread is let go —
/// it touches nothing but its log and a status it shares ownership of —
/// and the commands it started finish on their own.
/// @thread_safety See the file header.
class EditorBuildJob {
public:
  EditorBuildJob() = default;
  ~EditorBuildJob();

  EditorBuildJob(const EditorBuildJob&) = delete;
  EditorBuildJob& operator=(const EditorBuildJob&) = delete;
  EditorBuildJob(EditorBuildJob&&) = delete;
  EditorBuildJob& operator=(EditorBuildJob&&) = delete;

  /// Start running @p commands, logging to @p log, which is emptied first.
  /// False — and nothing started — when a build is already running.
  bool start(std::vector<EditorBuildCommand> commands,
             const std::filesystem::path& log);

  /// Where the build is. Once it has finished, its thread is joined.
  [[nodiscard]] EditorBuildStatus status();

private:
  /// Join the thread, if there is one to join.
  void join();

  /// The thread running the commands, while one is.
  std::thread thread_;
  /// Where the build is, written by the thread when it finishes. Shared
  /// with the thread, so a thread let go by the destructor still has it.
  std::shared_ptr<std::atomic<uint8_t>> status_ =
      std::make_shared<std::atomic<uint8_t>>(
          static_cast<uint8_t>(EditorBuildStatus::IDLE));
};

}  // namespace eng::editor
