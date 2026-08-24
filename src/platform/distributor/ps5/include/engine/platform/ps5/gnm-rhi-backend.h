#pragma once

// Design Summary -- GNM RHI Backend
// Technical Approach:
// docs/technical-approaches/engine/platform-ps5/gnm-rhi-backend.md
//
// Behaviours:
//   - Implement RhiDevice interface using GNM/GNMX PS5 graphics APIs
//   - Manage GPU resources with unified memory awareness (skip staging copies)
//   - Enable DCC and primitive shaders via GfxContext flags
//   - Enforce configurable GPU memory budget with LRU eviction
//   - Integrate AMD FSR 2 for temporal upscaling (DLSS disabled on PS5)
//   - Use Direct Storage for async texture/chunk streaming
//   - Support Performance (60 FPS / 1440p) and Quality (30 FPS / 4K) modes
//
// Edge Cases:
//   - GPU memory budget exceeded: LRU eviction before allocation failure
//   - GNM context creation failure: create() returns nullopt
//   - Direct Storage failure: fall back to CPU decompression
//   - FSR 2 unavailable: fall back to native resolution
//   - DCC creation failure: create without DCC, log warning
//
// Invariants:
//   - GNM/GNMX SDK headers never included in this public header
//   - All GPU allocations tracked against configurable budget
//   - No Vulkan/DX12/OpenGL code paths in PS5 builds
//   - Same RhiDevice interface as desktop backends
//
// Integration Points:
//   - RhiDevice interface: GnmRhiBackend is the PS5 concrete implementation
//   - Chunk streaming: Direct Storage for async I/O
//   - Upscaling: FSR 2 (DLSS force-disabled by CMake)
//   - Render pipeline: all render passes through RHI

#include "ps5-gnm-config.h"
#include "ps5-types.h"

#include <cstdint>
#include <memory>
#include <optional>

namespace eng {

// Forward declarations
class RhiDevice;

// ---------------------------------------------------------------------------
// GNM RHI Backend
// ---------------------------------------------------------------------------

/// PS5 GNM implementation of the RhiDevice interface.
/// Created via static factory; returns nullopt on GNM init failure.
/// All methods are main thread only unless documented otherwise.
///
/// Internal GNM/GNMX state (GfxContext, allocators, command pools)
/// is managed in the .cpp file and not exposed here.
class GnmRhiBackend {
public:
  /// Create and initialise the GNM backend. Returns nullopt if
  /// GNM device initialisation fails.
  /// Main thread only.
  static std::optional<GnmRhiBackend> create(const Ps5GnmConfig& config);

  ~GnmRhiBackend();

  GnmRhiBackend(GnmRhiBackend&& other) noexcept;
  GnmRhiBackend& operator=(GnmRhiBackend&& other) noexcept;

  GnmRhiBackend(const GnmRhiBackend&) = delete;
  GnmRhiBackend& operator=(const GnmRhiBackend&) = delete;

  /// Returns the current GPU memory usage in bytes.
  uint64_t gpuMemoryUsedBytes() const;

  /// Returns the configured GPU memory budget in bytes.
  uint64_t gpuMemoryBudgetBytes() const;

  /// Returns the active render mode.
  Ps5RenderMode renderMode() const;

  /// Switch between Performance and Quality render modes at runtime.
  /// Main thread only.
  void setRenderMode(Ps5RenderMode mode);

private:
  GnmRhiBackend();

  struct Impl;
  /// Opaque implementation holding GNM/GNMX state.
  std::unique_ptr<Impl> impl_;
};

}  // namespace eng
