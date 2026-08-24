#pragma once

// Design Summary -- Xbox Series X DX12 Helpers
// Technical Approach:
// docs/technical-approaches/engine/platform-xbox-series-x/dx12-helpers.md
//
// Behaviours:
//   - Create Xbox D3D12 device via GDK D3D12.x extensions
//   - Create Xbox swap chain (not via DXGI factory enumeration)
//   - Allocate GPU memory from fast pool (render targets, textures)
//   - Allocate GPU memory from standard pool (CPU data, audio)
//   - Initialise DirectStorage for async chunk loading
//   - Shut down all Xbox DX12 resources
//
// Edge Cases:
//   - Device creation failure: return XboxError::DEVICE_CREATION_FAILED
//   - Memory pool exhaustion: return error, caller uses lower-quality assets
//   - DirectStorage init failure: fall back to CPU async I/O
//   - SFS unavailable on debug kit: graceful fallback to standard loading
//   - Swap chain present during suspend: no-op
//
// Invariants:
//   - Xbox D3D12 device created once at startup, never recreated
//   - Fast pool used exclusively for GPU render targets and textures
//   - DirectStorage queue created once, shared across all chunk loading
//   - All Xbox D3D12.x calls gated behind ENGINE_PLATFORM_XBOX_SERIES_X
//   - Shader model minimum: sm_6_0
//
// Integration Points:
//   - Dx12RhiBackend: calls these helpers conditionally on Xbox builds
//   - World streaming: DirectStorage queue for async chunk I/O
//   - Texture streaming: SFS for on-demand tile loading

#include "xbox-dx12-config.h"
#include "xbox-dx12-resources.h"
#include "xbox-types.h"

#include <cstdint>
#include <engine/core/expected-polyfill.h>

namespace eng {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

/// Create the Xbox D3D12 device using GDK D3D12.x extensions.
/// Uses D3D12XBOX_CREATE_DEVICE_FLAG instead of DXGI factory enumeration.
/// Returns populated resources on success.
/// Main thread only.
std::expected<XboxDx12Resources, XboxError>
createXboxDevice(const XboxDx12Config& config);

/// Create the Xbox swap chain. Must be called after createXboxDevice().
/// Main thread only.
std::expected<bool, XboxError>
createXboxSwapChain(XboxDx12Resources& resources, const XboxDx12Config& config);

/// Allocate memory from the specified Xbox memory pool.
/// FAST: 560 GB/s pool for GPU resources.
/// STANDARD: 336 GB/s pool for CPU-side data.
/// Main thread only.
std::expected<bool, XboxError>
allocateXboxMemoryPool(XboxDx12Resources& resources, XboxMemoryPool pool,
                       uint64_t size_bytes);

/// Initialise DirectStorage for async chunk loading. Creates the
/// IDStorageFactory and IDStorageQueue. Falls back gracefully if
/// DirectStorage is unavailable.
/// Main thread only.
std::expected<bool, XboxError>
initXboxDirectStorage(XboxDx12Resources& resources);

/// Shut down all Xbox DX12 resources. Releases device, swap chain,
/// memory pools, and DirectStorage queue. Idempotent.
/// Main thread only.
void shutdownXboxDx12(XboxDx12Resources& resources);

}  // namespace eng
