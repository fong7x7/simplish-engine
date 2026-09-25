#include <cstdlib>
#include <editor/build/editor-build-commands.h>
#include <editor/build/editor-build-job.h>
#include <fstream>
#include <system_error>
#include <utility>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace eng::editor {

namespace {

  /// Append the command about to run to @p log, as a shell prompt would
  /// show it, so the log reads as a transcript.
  void announce(const std::filesystem::path& log,
                const EditorBuildCommand& command) {
    std::ofstream out(log, std::ios::app);
    out << "$";
    for (const std::string& word : command.words) {
      out << ' ' << word;
    }
    out << '\n';
  }

  /// The exit code in what `std::system` returned: on POSIX a wait
  /// status, from which the code is taken; on Windows the code itself.
  int exitCode(int status) {
#ifdef _WIN32
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
  }

  /// Append to @p log that @p command failed with @p status, in words a
  /// person — and `buildErrorLines` — reads as an error. The shell reports
  /// a program killed by a signal as 128 plus the signal: 139 is a crash.
  void reportFailure(const std::filesystem::path& log,
                     const EditorBuildCommand& command, int status) {
    const int code = exitCode(status);
    std::ofstream out(log, std::ios::app);
    out << "error: " << command.words.front() << " failed (exit code "
        << code << (code > 128 ? ": it crashed" : "") << ")\n";
  }

  /// @p status as the atomic holds it.
  uint8_t stored(EditorBuildStatus status) {
    return static_cast<uint8_t>(status);
  }

  /// A build thread's body: each command in turn, until one fails, then
  /// how it went into @p status. Owns everything it touches.
  void runCommands(std::vector<EditorBuildCommand> commands,
                   std::filesystem::path log,
                   std::shared_ptr<std::atomic<uint8_t>> status) {
    for (const EditorBuildCommand& command : commands) {
      announce(log, command);
      // The one thread that runs a build's commands, each word of which
      // shellLine has quoted; running a command line is the point.
      // NOLINTNEXTLINE(concurrency-mt-unsafe,cert-env33-c,bugprone-command-processor)
      const int exit = std::system(shellLine(command, log).c_str());
      if (exit != 0) {
        reportFailure(log, command, exit);
        status->store(stored(EditorBuildStatus::FAILED));
        return;
      }
    }
    status->store(stored(EditorBuildStatus::SUCCEEDED));
  }

}  // namespace

EditorBuildJob::~EditorBuildJob() {
  if (thread_.joinable() &&
      status_->load() == stored(EditorBuildStatus::RUNNING)) {
    thread_.detach();
  }
  join();
}

bool EditorBuildJob::start(std::vector<EditorBuildCommand> commands,
                           const std::filesystem::path& log) {
  if (status() == EditorBuildStatus::RUNNING) {
    return false;
  }
  join();
  std::error_code ec;
  std::filesystem::create_directories(log.parent_path(), ec);
  std::ofstream(log, std::ios::trunc).flush();
  status_->store(stored(EditorBuildStatus::RUNNING));
  thread_ = std::thread(runCommands, std::move(commands), log, status_);
  return true;
}

EditorBuildStatus EditorBuildJob::status() {
  const auto now = static_cast<EditorBuildStatus>(status_->load());
  if (now != EditorBuildStatus::RUNNING) {
    join();
  }
  return now;
}

void EditorBuildJob::join() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

}  // namespace eng::editor
