#include "dx12-descriptor-heap-allocator.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/dx12-backend.md §4
//   — Dx12DescriptorHeapAllocator for descriptor heap index management

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST_CASE("DescriptorHeapAllocator: default constructed has zero capacity",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc;
  REQUIRE(alloc.capacity() == 0);
}

TEST_CASE("DescriptorHeapAllocator: constructed with capacity stores it",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(128);
  REQUIRE(alloc.capacity() == 128);
}

// ---------------------------------------------------------------------------
// Allocation
// ---------------------------------------------------------------------------

TEST_CASE("DescriptorHeapAllocator: first allocate returns index 0",
          "[dx12][descriptor-heap]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §4
  //   — Index 0 is valid for descriptor heaps
  Dx12DescriptorHeapAllocator alloc(64);
  REQUIRE(alloc.allocate() == 0);
}

TEST_CASE("DescriptorHeapAllocator: sequential allocations return incrementing "
          "indices",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(64);
  REQUIRE(alloc.allocate() == 0);
  REQUIRE(alloc.allocate() == 1);
  REQUIRE(alloc.allocate() == 2);
}

TEST_CASE("DescriptorHeapAllocator: exhaustion returns UINT32_MAX",
          "[dx12][descriptor-heap]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §6
  //   — Resource allocation failure returns sentinel
  Dx12DescriptorHeapAllocator alloc(2);
  REQUIRE(alloc.allocate() == 0);
  REQUIRE(alloc.allocate() == 1);
  REQUIRE(alloc.allocate() == UINT32_MAX);
}

TEST_CASE("DescriptorHeapAllocator: zero capacity exhausts immediately",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(0);
  REQUIRE(alloc.allocate() == UINT32_MAX);
}

// ---------------------------------------------------------------------------
// Free + Recycle
// ---------------------------------------------------------------------------

TEST_CASE("DescriptorHeapAllocator: freed index is recycled on next allocate",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(4);
  uint32_t idx0 = alloc.allocate();
  alloc.allocate();  // idx 1

  alloc.free(idx0);

  uint32_t recycled = alloc.allocate();
  REQUIRE(recycled == idx0);
}

TEST_CASE("DescriptorHeapAllocator: freed indices recycle in LIFO order",
          "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(8);
  uint32_t idx0 = alloc.allocate();
  uint32_t idx1 = alloc.allocate();
  uint32_t idx2 = alloc.allocate();

  alloc.free(idx0);
  alloc.free(idx1);
  alloc.free(idx2);

  REQUIRE(alloc.allocate() == idx2);
  REQUIRE(alloc.allocate() == idx1);
  REQUIRE(alloc.allocate() == idx0);
}

TEST_CASE(
    "DescriptorHeapAllocator: free then allocate recovers from exhaustion",
    "[dx12][descriptor-heap]") {
  Dx12DescriptorHeapAllocator alloc(1);
  uint32_t idx = alloc.allocate();
  REQUIRE(alloc.allocate() == UINT32_MAX);

  alloc.free(idx);
  REQUIRE(alloc.allocate() == idx);
}

// ---------------------------------------------------------------------------
// Stress
// ---------------------------------------------------------------------------

TEST_CASE("DescriptorHeapAllocator: full allocate-free-reallocate cycle",
          "[dx12][descriptor-heap][property]") {
  constexpr uint32_t CAPACITY = 64;
  Dx12DescriptorHeapAllocator alloc(CAPACITY);

  std::vector<uint32_t> indices;
  indices.reserve(CAPACITY);
  for (uint32_t i = 0; i < CAPACITY; ++i) {
    indices.push_back(alloc.allocate());
    REQUIRE(indices.back() != UINT32_MAX);
  }
  REQUIRE(alloc.allocate() == UINT32_MAX);

  for (auto idx : indices) {
    alloc.free(idx);
  }

  for (uint32_t i = 0; i < CAPACITY; ++i) {
    REQUIRE(alloc.allocate() != UINT32_MAX);
  }
  REQUIRE(alloc.allocate() == UINT32_MAX);
}
