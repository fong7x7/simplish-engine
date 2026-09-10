#pragma once

#include <cstdlib>
#include <engine/core/logger.h>
#include <string>
#include <string_view>

// ============================================================================
// ENGINE_ASSERT — precondition / postcondition assertion macro.
//
// Usage:
//   ENGINE_ASSERT(ptr != nullptr, "context must be initialized");
//
// Behaviour:
//   - In all builds: logs an error and calls std::abort() on failure.
//   - The message must be an actionable description of what went wrong.
//   - Use only for programming errors (invariant violations), never for
//     recoverable errors (use std::optional / error codes instead).
//
// Thread Safety: safe to call from any thread.
// ============================================================================

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,cppcoreguidelines-avoid-do-while)
// -- macro required for __FILE__ / __LINE__; do-while is the standard
// multi-statement macro idiom
#define ENGINE_ASSERT(condition, message)                                      \
  do {                                                                         \
    if (!(condition)) {                                                        \
      ::eng::Logger::error("ASSERT", std::string(message) + " [" + __FILE__ +  \
                                         ":" + std::to_string(__LINE__) +      \
                                         "]");                                 \
      std::abort();                                                            \
    }                                                                          \
  } while (false)
