#ifdef ENGINE_RENDERER_DX12

#include "dx12-copy-alignment.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <d3d12.h>

using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/dx12-backend.md §4
//   — Buffer/texture copies use D3D12's required placed-footprint pitch

// ---------------------------------------------------------------------------
// Row pitch alignment
// ---------------------------------------------------------------------------

TEST_CASE("dx12AlignRowPitch: leaves an already aligned pitch alone",
          "[dx12][copy-alignment]") {
  REQUIRE(dx12AlignRowPitch(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) ==
          D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  REQUIRE(dx12AlignRowPitch(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * 3) ==
          D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * 3);
}

TEST_CASE("dx12AlignRowPitch: rounds a short row up to the next boundary",
          "[dx12][copy-alignment]") {
  REQUIRE(dx12AlignRowPitch(1) == D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
  REQUIRE(dx12AlignRowPitch(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT + 1) ==
          D3D12_TEXTURE_DATA_PITCH_ALIGNMENT * 2);
}

TEST_CASE("dx12AlignRowPitch: zero bytes need no padding",
          "[dx12][copy-alignment]") {
  REQUIRE(dx12AlignRowPitch(0) == 0);
}

TEST_CASE("dx12AlignRowPitch: a typical RGBA8 row gains padding",
          "[dx12][copy-alignment]") {
  // 100 px of RGBA8 is 400 bytes, which D3D12 will only copy at 512.
  constexpr uint32_t row_bytes = 100 * 4;
  const uint32_t pitch = dx12AlignRowPitch(row_bytes);
  REQUIRE(pitch == 512);
  REQUIRE(pitch >= row_bytes);
  REQUIRE(pitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);
}

#endif  // ENGINE_RENDERER_DX12
