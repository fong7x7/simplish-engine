#pragma once

#include <cstdint>
#include <functional>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Logger: Centralized logging with level filtering.
//
// Responsibilities:
// - Provide thread-safe logging across engine subsystems
// - Filter by log level (debug, info, warn, error)
// - Write to console and/or file (configurable)
// - Include timestamps and context (thread, subsystem)
//
// Key Invariants:
// - Logging is always available (even if engine not initialized)
// - Log level filtering is consistent across all subsystems
// - No exceptions thrown by logging operations
// - Logging is non-blocking (best-effort async if needed)
//
// Thread Safety:
// - log_*: safe from any thread
// - set_level: safe from any thread (atomic)
// - File writes: buffered or async to avoid blocking simulation
// ============================================================================

enum class LogLevel {
  DEBUG = 0,
  INFO = 1,
  WARN = 2,
  ERROR = 3,
};

/// Toggle for file logging (replaces bare bool parameter).
enum class FileLogging { ENABLED, DISABLED };

class Logger {
public:
  // Log at debug level
  // Format: "[timestamp] [DEBUG] [subsystem] message"
  static void debug(std::string_view subsystem, std::string_view message);

  // Log at info level
  static void info(std::string_view subsystem, std::string_view message);

  // Log at warn level
  static void warn(std::string_view subsystem, std::string_view message);

  // Log at error level
  static void error(std::string_view subsystem, std::string_view message);

  // Set global log level (all messages below this level are filtered)
  static void setLevel(LogLevel level);

  // Get current log level
  static LogLevel level();

  // Enable/disable file logging
  static void setFileLogging(std::string_view log_file_path, FileLogging mode);

  /// Callback invoked for each log message: (level, subsystem, message).
  using SinkFn = std::function<void(
      LogLevel, std::string_view,
      std::string_view)>;  // NOLINT(misc-confusable-identifiers)

  /// Opaque handle returned by registerSink for later removal.
  using SinkHandle = uint64_t;

  /// Register a custom log sink. Returns a handle for unregisterSink.
  /// @threading Safe from any thread (mutex-guarded).
  static SinkHandle registerSink(SinkFn sink);

  /// Remove a previously registered sink by handle.
  /// @threading Safe from any thread (mutex-guarded).
  static void unregisterSink(SinkHandle handle);
};

// Convenience macros for common subsystems (optional)
#define LOG_DEBUG(subsystem, msg) eng::Logger::debug(subsystem, msg)
#define LOG_INFO(subsystem, msg) eng::Logger::info(subsystem, msg)
#define LOG_WARN(subsystem, msg) eng::Logger::warn(subsystem, msg)
#define LOG_ERROR(subsystem, msg) eng::Logger::error(subsystem, msg)

}  // namespace eng
