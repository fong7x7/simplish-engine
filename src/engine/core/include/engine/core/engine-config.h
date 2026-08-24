#pragma once

#include <cstdint>
#include <string>

namespace eng {

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// EngineConfig: Configuration for engine initialization.
//
// Responsibilities:
// - Define engine mode (full vs headless)
// - Specify data directories, threading, performance, and graphics options
// - Provide determinism and logging settings
//
// Key Invariants:
// - All fields have sensible defaults
// - Config is read-only after engine construction
// - No environment-specific hardcoded values
// ============================================================================

// World-space scalar coordinates for C ABI / config (grid types ChunkCoord,
// VoxelCoord live in engine/voxel — do not alias those names here).
using WorldVoxelCoord = int64_t;
using EntityID = uint64_t;
using SubscriptionHandle = uint64_t;
using TaskID = uint64_t;

inline constexpr EntityID ENTITY_ID_INVALID = 0;
inline constexpr TaskID TASK_ID_INVALID = 0;
inline constexpr int32_t CHUNK_SIZE = 32;

// Engine mode: full (all subsystems) or headless (dedicated server)
enum class EngineMode : uint8_t {
  FULL,      // All subsystems initialized
  HEADLESS,  // No rendering, audio, input, or GUI (dedicated server)
};

struct EngineConfig {
  /// Engine mode (full or headless for dedicated server).
  EngineMode mode = EngineMode::FULL;

  /// Root data directory for assets and configuration files.
  std::string data_dir{};
  /// Path to the SQLite world database file.
  std::string world_db_path{};
  /// Directory for region pool files (chunk storage on disk).
  std::string region_pool_dir{};

  /// Number of worker threads; 0 means auto-detect (core count - 2).
  uint32_t num_worker_threads = 0;
  /// Whether rendering runs on the main thread instead of a dedicated thread.
  bool render_on_main_thread = false;

  /// Maximum memory for the chunk cache in megabytes.
  uint32_t chunk_cache_size_mb = 512;
  /// Maximum number of chunks held in memory simultaneously.
  uint32_t max_chunks_loaded = 4096;
  /// Maximum chunk integrations processed per frame tick.
  uint32_t max_chunk_integrations_per_frame = 8;

  /// Enable DLSS upscaling (ignored in headless mode).
  bool enable_dlss = false;
  /// Enable hardware ray tracing (ignored in headless mode).
  bool enable_ray_tracing = false;

  /// World seed; 0 means random, nonzero for deterministic generation.
  uint64_t world_seed = 0;
  /// Use fixed-step physics with deterministic ordering.
  bool deterministic_physics = false;

  /// Minimum log level string ("debug", "info", "warn", "error").
  std::string log_level = "info";
};

}  // namespace eng
