#include <atomic>
#include <cstdio>
#include <engine/core/logger.h>
#include <fstream>
#include <mutex>
#include <vector>

namespace eng {

namespace {

  /// Global log level — atomic for lock-free reads from any thread.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  std::atomic<LogLevel> g_log_level{LogLevel::INFO};
  /// Mutex protecting file output and sinks.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  std::mutex g_log_mutex;
  /// File output stream for file logging.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,bugprone-throwing-static-initialization,cert-err58-cpp)
  std::ofstream g_log_file;

  /// Registered sink entry with handle for removal.
  struct SinkEntry {
    /// Unique handle for unregisterSink.
    Logger::SinkHandle handle = 0;
    /// Callback function.
    Logger::SinkFn fn;
  };

  /// All registered log sinks. Protected by g_log_mutex.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  std::vector<SinkEntry> g_sinks;
  /// Monotonic counter for sink handles.
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
  Logger::SinkHandle g_next_sink_handle{1};

  const char* levelTag(LogLevel level) {
    switch (level) {
      case LogLevel::DEBUG:
        return "DEBUG";
      case LogLevel::INFO:
        return "INFO";
      case LogLevel::WARN:
        return "WARN";
      case LogLevel::ERROR:
        return "ERROR";
    }
    return "UNKNOWN";
  }

  /// Write a log entry to the file output if open.
  void writeToLogFile(LogLevel level, std::string_view subsystem,
                      std::string_view message) {
    if (!g_log_file.is_open()) {
      return;
    }
    g_log_file << "[" << levelTag(level) << "] " << subsystem << ": " << message
               << "\n";
  }

  /// Dispatch to all registered sinks (called under g_log_mutex).
  void dispatchSinks(LogLevel level, std::string_view subsystem,
                     std::string_view message) {
    for (const auto& entry : g_sinks) {
      if (entry.fn) {
        entry.fn(level, subsystem, message);
      }
    }
  }

  void logMessage(LogLevel level, std::string_view subsystem,
                  std::string_view message) {
    if (level < g_log_level.load(std::memory_order_relaxed)) {
      return;
    }
    const std::scoped_lock lock(g_log_mutex);
    // NOLINTNEXTLINE(hicpp-vararg,cert-err33-c,modernize-use-std-print)
    std::fprintf(stderr, "[%s] %.*s: %.*s\n", levelTag(level),
                 static_cast<int>(subsystem.size()), subsystem.data(),
                 static_cast<int>(message.size()), message.data());
    writeToLogFile(level, subsystem, message);
    dispatchSinks(level, subsystem, message);
  }

}  // namespace

void Logger::debug(std::string_view subsystem, std::string_view message) {
  logMessage(LogLevel::DEBUG, subsystem, message);
}

void Logger::info(std::string_view subsystem, std::string_view message) {
  logMessage(LogLevel::INFO, subsystem, message);
}

void Logger::warn(std::string_view subsystem, std::string_view message) {
  logMessage(LogLevel::WARN, subsystem, message);
}

void Logger::error(std::string_view subsystem, std::string_view message) {
  logMessage(LogLevel::ERROR, subsystem, message);
}

void Logger::setLevel(LogLevel level) {
  g_log_level.store(level, std::memory_order_relaxed);
}

LogLevel Logger::level() {
  return g_log_level.load(std::memory_order_relaxed);
}

void Logger::setFileLogging(std::string_view log_file_path, FileLogging mode) {
  const std::scoped_lock lock(g_log_mutex);
  if (mode == FileLogging::ENABLED) {
    g_log_file.open(std::string(log_file_path), std::ios::out | std::ios::app);
  } else {
    g_log_file.close();
  }
}

Logger::SinkHandle Logger::registerSink(SinkFn sink) {
  const std::scoped_lock lock(g_log_mutex);
  const auto handle = g_next_sink_handle++;
  g_sinks.push_back({handle, std::move(sink)});
  return handle;
}

void Logger::unregisterSink(SinkHandle handle) {
  const std::scoped_lock lock(g_log_mutex);
  std::erase_if(g_sinks,
                [handle](const SinkEntry& e) { return e.handle == handle; });
}

}  // namespace eng
