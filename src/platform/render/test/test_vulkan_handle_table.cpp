#include "../../../render/backends/vulkan/src/vulkan-handle-table.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
//   — VulkanHandleTable generational handle table

// ---------------------------------------------------------------------------
// Insert + Lookup
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: insert returns non-zero handle",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Opaque handles; 0 is invalid
  VulkanHandleTable<int> table;
  auto h = table.insert(42);
  REQUIRE(h != 0);
}

TEST_CASE("HandleTable: lookup returns inserted value",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Resource handle lookup
  VulkanHandleTable<int> table;
  auto h = table.insert(99);
  auto* val = table.lookup(h);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 99);
}

TEST_CASE("HandleTable: multiple inserts return distinct handles",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Unique resource handles
  VulkanHandleTable<int> table;
  auto h1 = table.insert(1);
  auto h2 = table.insert(2);
  auto h3 = table.insert(3);
  REQUIRE(h1 != h2);
  REQUIRE(h2 != h3);
  REQUIRE(h1 != h3);
}

TEST_CASE("HandleTable: lookup with zero handle returns nullptr",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Handle 0 is always invalid
  VulkanHandleTable<int> table;
  REQUIRE(table.lookup(0) == nullptr);
}

TEST_CASE("HandleTable: lookup with bogus handle returns nullptr",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.3
  //   — I5: generational indices detect use-after-free
  VulkanHandleTable<int> table;
  table.insert(1);
  REQUIRE(table.lookup(0xDEADBEEF) == nullptr);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: remove returns the resource",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Resource destruction
  VulkanHandleTable<std::string> table;
  auto h = table.insert("hello");
  auto removed = table.remove(h);
  REQUIRE(removed.has_value());
  REQUIRE(*removed == "hello");
}

TEST_CASE("HandleTable: lookup after remove returns nullptr",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.3
  //   — I5: stale handles are rejected
  VulkanHandleTable<int> table;
  auto h = table.insert(42);
  table.remove(h);
  REQUIRE(table.lookup(h) == nullptr);
}

TEST_CASE("HandleTable: double remove returns nullopt",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.2
  //   — E8: destroy with invalid handle is no-op
  VulkanHandleTable<int> table;
  auto h = table.insert(42);
  auto first = table.remove(h);
  auto second = table.remove(h);
  REQUIRE(first.has_value());
  REQUIRE_FALSE(second.has_value());
}

TEST_CASE("HandleTable: remove with zero handle returns nullopt",
          "[vulkan][handle-table]") {
  // Req: docs/engine/rendering.md §2 — destroy*() safe with invalid handles
  VulkanHandleTable<int> table;
  REQUIRE_FALSE(table.remove(0).has_value());
}

// ---------------------------------------------------------------------------
// Generation / Stale Handle Detection
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: stale handle after remove-then-insert is rejected",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.3
  //   — I5: generational indices detect use-after-free
  VulkanHandleTable<int> table;
  auto old_handle = table.insert(100);
  table.remove(old_handle);
  auto new_handle = table.insert(200);

  // Old handle must not resolve to the new resource
  REQUIRE(table.lookup(old_handle) == nullptr);

  // New handle should resolve correctly
  auto* val = table.lookup(new_handle);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 200);
}

TEST_CASE("HandleTable: slot recycling reuses freed indices",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Recycle slots for constant-time insert/remove
  VulkanHandleTable<int> table;
  auto h1 = table.insert(1);
  auto h2 = table.insert(2);

  table.remove(h1);

  // Next insert should recycle the freed slot
  auto h3 = table.insert(3);

  // h3 should be a different handle from h1 (different generation)
  REQUIRE(h3 != h1);

  // h3 should still be valid
  auto* val = table.lookup(h3);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 3);

  // h2 should still be valid
  auto* val2 = table.lookup(h2);
  REQUIRE(val2 != nullptr);
  REQUIRE(*val2 == 2);
}

// ---------------------------------------------------------------------------
// forEachAlive
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: forEachAlive visits all alive resources",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Handle table supports iteration for shutdown cleanup
  VulkanHandleTable<int> table;
  table.insert(10);
  table.insert(20);
  table.insert(30);

  std::vector<int> visited;
  table.forEachAlive([&](int& val) { visited.push_back(val); });
  REQUIRE(visited.size() == 3);
  REQUIRE(visited[0] == 10);
  REQUIRE(visited[1] == 20);
  REQUIRE(visited[2] == 30);
}

TEST_CASE("HandleTable: forEachAlive skips removed resources",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Removed slots are not visited
  VulkanHandleTable<int> table;
  table.insert(10);
  auto h2 = table.insert(20);
  table.insert(30);

  table.remove(h2);

  std::vector<int> visited;
  table.forEachAlive([&](int& val) { visited.push_back(val); });
  REQUIRE(visited.size() == 2);
  REQUIRE(visited[0] == 10);
  REQUIRE(visited[1] == 30);
}

TEST_CASE("HandleTable: forEachAlive on empty table visits nothing",
          "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Empty table edge case
  VulkanHandleTable<int> table;
  int count = 0;
  table.forEachAlive([&](int&) { ++count; });
  REQUIRE(count == 0);
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: works with move-only types", "[vulkan][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §5
  //   — Handle table stores Vulkan resource wrappers (move-only)
  VulkanHandleTable<std::unique_ptr<int>> table;
  auto h = table.insert(std::make_unique<int>(42));

  auto* val = table.lookup(h);
  REQUIRE(val != nullptr);
  REQUIRE(**val == 42);

  auto removed = table.remove(h);
  REQUIRE(removed.has_value());
  REQUIRE(**removed == 42);
}

// ---------------------------------------------------------------------------
// Stress / property-like
// ---------------------------------------------------------------------------

TEST_CASE("HandleTable: bulk insert-remove-reinsert cycle",
          "[vulkan][handle-table][property]") {
  // Req: docs/technical-approaches/engine/rendering/vulkan-backend.md §1.3
  //   — I5: generational indices; no stale handle collisions under load
  constexpr int COUNT = 100;
  VulkanHandleTable<int> table;

  // Insert COUNT items
  std::vector<uint64_t> handles;
  handles.reserve(COUNT);
  for (int i = 0; i < COUNT; ++i) {
    handles.push_back(table.insert(i));
  }

  // Remove all
  for (auto h : handles) {
    REQUIRE(table.remove(h).has_value());
  }

  // Verify all old handles are stale
  for (auto h : handles) {
    REQUIRE(table.lookup(h) == nullptr);
  }

  // Reinsert — should recycle slots
  std::vector<uint64_t> new_handles;
  new_handles.reserve(COUNT);
  for (int i = 0; i < COUNT; ++i) {
    new_handles.push_back(table.insert(i + 1000));
  }

  // All new handles must be valid and distinct from old
  for (size_t i = 0; i < static_cast<size_t>(COUNT); ++i) {
    auto* val = table.lookup(new_handles[i]);
    REQUIRE(val != nullptr);
    REQUIRE(*val == static_cast<int>(i) + 1000);
    REQUIRE(new_handles[i] != handles[i]);
  }
}
