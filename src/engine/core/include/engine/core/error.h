#pragma once

#include "engine-config.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// Error codes and utilities for engine APIs.
//
// Responsibilities:
// - Define standard error codes used across engine
// - Provide error logging utilities
// - Distinguish between contract violations, missing resources, and I/O
// failures
//
// Key Invariants:
// - Error code 0 (OK) always means success
// - Non-zero error codes are failures
// - Every API returning error codes documents which codes it may return
// - Errors are logged before return (with context)
// - Optional<T> is preferred over error codes for simpler APIs
// ============================================================================

// Standard engine error codes
enum class ErrorCode : uint32_t {
  OK = 0,

  // Contract violations (invalid arguments)
  INVALID_ARGUMENT = 1,
  NULL_POINTER_DEREFERENCE = 2,
  INVALID_ENTITY_ID = 3,
  INVALID_VOXEL_TYPE_ID = 4,
  INVALID_CHUNK_COORDINATE = 5,

  // Missing resources
  RESOURCE_NOT_FOUND = 10,
  VOXEL_TYPE_NOT_REGISTERED = 11,
  CHUNK_NOT_LOADED = 12,
  ENTITY_NOT_FOUND = 13,
  DEFINITION_NOT_FOUND = 14,

  // I/O failures
  IO_ERROR = 20,
  DISK_FULL = 21,
  PERMISSION_DENIED = 22,
  CORRUPTED_DATA = 23,
  FILE_NOT_FOUND = 24,

  // Plugin failures
  PLUGIN_INIT_FAILED = 30,
  PLUGIN_NOT_LOADED = 31,
  PLUGIN_UNHEALTHY = 32,
  PLUGIN_SYMBOL_NOT_FOUND = 33,

  // Initialization failures
  INIT_PHASE_ALREADY_COMPLETED = 40,
  INIT_DEPENDENCY_NOT_MET = 41,
  INIT_SEQUENCE_VIOLATION = 42,

  // Threading violations
  WRONG_THREAD = 50,
  RACE_CONDITION = 51,

  // Generic error
  UNKNOWN = 255,
};

// Sentinel values for APIs that return IDs
inline constexpr uint32_t VOXEL_TYPE_INVALID = 0;
inline constexpr uint64_t SUBSCRIPTION_HANDLE_INVALID = 0;
inline constexpr int64_t CHUNK_COORD_INVALID = INT64_MIN;

// Error context for logging with caller context
struct ErrorContext {
  /// Error code indicating the type of failure.
  ErrorCode code;
  /// Human-readable error message with details.
  std::string_view message;
  /// Name of the operation that failed (e.g. "voxel_set", "chunk_load").
  std::string_view operation;
  /// Path of the file involved in the error, if applicable.
  std::string_view file_path;
  /// Entity ID involved in the error, or ENTITY_ID_INVALID if none.
  uint64_t entity_id = ENTITY_ID_INVALID;
};

}  // namespace eng
