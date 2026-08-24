#include "../../../render/backends/dx12/src/dx12-handle-table.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace eng::render;

// Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
//   — Dx12HandleTable generational handle table

// ---------------------------------------------------------------------------
// Insert + Lookup
// ---------------------------------------------------------------------------

TEST_CASE("Dx12HandleTable: insert returns non-zero handle",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Opaque handles; 0 is invalid
  Dx12HandleTable<int> table;
  auto h = table.insert(42);
  REQUIRE(h != 0);
}

TEST_CASE("Dx12HandleTable: lookup returns inserted value",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Resource handle lookup
  Dx12HandleTable<int> table;
  auto h = table.insert(99);
  auto* val = table.lookup(h);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 99);
}

TEST_CASE("Dx12HandleTable: multiple inserts return distinct handles",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Unique resource handles
  Dx12HandleTable<int> table;
  auto h1 = table.insert(1);
  auto h2 = table.insert(2);
  auto h3 = table.insert(3);
  REQUIRE(h1 != h2);
  REQUIRE(h2 != h3);
  REQUIRE(h1 != h3);
}

TEST_CASE("Dx12HandleTable: lookup with zero handle returns nullptr",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Handle 0 is always invalid
  Dx12HandleTable<int> table;
  REQUIRE(table.lookup(0) == nullptr);
}

TEST_CASE("Dx12HandleTable: lookup with bogus handle returns nullptr",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — generational indices detect use-after-free
  Dx12HandleTable<int> table;
  table.insert(1);
  REQUIRE(table.lookup(0xDEADBEEF) == nullptr);
}

// ---------------------------------------------------------------------------
// Remove
// ---------------------------------------------------------------------------

TEST_CASE("Dx12HandleTable: remove returns the resource",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — Resource destruction
  Dx12HandleTable<std::string> table;
  auto h = table.insert("hello");
  auto removed = table.remove(h);
  REQUIRE(removed.has_value());
  REQUIRE(*removed == "hello");
}

TEST_CASE("Dx12HandleTable: lookup after remove returns nullptr",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — stale handles are rejected
  Dx12HandleTable<int> table;
  auto h = table.insert(42);
  table.remove(h);
  REQUIRE(table.lookup(h) == nullptr);
}

TEST_CASE("Dx12HandleTable: double remove returns nullopt",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §6
  //   — destroy with invalid handle is no-op
  Dx12HandleTable<int> table;
  auto h = table.insert(42);
  auto first = table.remove(h);
  auto second = table.remove(h);
  REQUIRE(first.has_value());
  REQUIRE_FALSE(second.has_value());
}

TEST_CASE("Dx12HandleTable: remove with zero handle returns nullopt",
          "[dx12][handle-table]") {
  // Req: docs/engine/rendering.md §2 — destroy*() safe with invalid handles
  Dx12HandleTable<int> table;
  REQUIRE_FALSE(table.remove(0).has_value());
}

// ---------------------------------------------------------------------------
// Generation / Stale Handle Detection
// ---------------------------------------------------------------------------

TEST_CASE("Dx12HandleTable: stale handle after remove-then-insert is rejected",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — generational indices detect use-after-free
  Dx12HandleTable<int> table;
  auto old_handle = table.insert(100);
  table.remove(old_handle);
  auto new_handle = table.insert(200);

  REQUIRE(table.lookup(old_handle) == nullptr);

  auto* val = table.lookup(new_handle);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 200);
}

TEST_CASE("Dx12HandleTable: slot recycling reuses freed indices",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — Recycle slots for constant-time insert/remove
  Dx12HandleTable<int> table;
  auto h1 = table.insert(1);
  auto h2 = table.insert(2);

  table.remove(h1);
  auto h3 = table.insert(3);

  REQUIRE(h3 != h1);

  auto* val = table.lookup(h3);
  REQUIRE(val != nullptr);
  REQUIRE(*val == 3);

  auto* val2 = table.lookup(h2);
  REQUIRE(val2 != nullptr);
  REQUIRE(*val2 == 2);
}

// ---------------------------------------------------------------------------
// forEachAlive
// ---------------------------------------------------------------------------

TEST_CASE("Dx12HandleTable: forEachAlive visits all alive resources",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — Handle table supports iteration for shutdown cleanup
  Dx12HandleTable<int> table;
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

TEST_CASE("Dx12HandleTable: forEachAlive skips removed resources",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — Removed slots are not visited
  Dx12HandleTable<int> table;
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

TEST_CASE("Dx12HandleTable: forEachAlive on empty table visits nothing",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — Empty table edge case
  Dx12HandleTable<int> table;
  int count = 0;
  table.forEachAlive([&](int&) { ++count; });
  REQUIRE(count == 0);
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST_CASE("Dx12HandleTable: works with move-only types",
          "[dx12][handle-table]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — Handle table stores resource wrappers (move-only)
  Dx12HandleTable<std::unique_ptr<int>> table;
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

TEST_CASE("Dx12HandleTable: bulk insert-remove-reinsert cycle",
          "[dx12][handle-table][property]") {
  // Req: docs/technical-approaches/engine/rendering/dx12-backend.md §2
  //   — generational indices; no stale handle collisions under load
  constexpr int COUNT = 100;
  Dx12HandleTable<int> table;

  std::vector<uint64_t> handles;
  handles.reserve(COUNT);
  for (int i = 0; i < COUNT; ++i) {
    handles.push_back(table.insert(i));
  }

  for (auto h : handles) {
    REQUIRE(table.remove(h).has_value());
  }

  for (auto h : handles) {
    REQUIRE(table.lookup(h) == nullptr);
  }

  std::vector<uint64_t> new_handles;
  new_handles.reserve(COUNT);
  for (int i = 0; i < COUNT; ++i) {
    new_handles.push_back(table.insert(i + 1000));
  }

  for (size_t i = 0; i < static_cast<size_t>(COUNT); ++i) {
    auto* val = table.lookup(new_handles[i]);
    REQUIRE(val != nullptr);
    REQUIRE(*val == static_cast<int>(i) + 1000);
    REQUIRE(new_handles[i] != handles[i]);
  }
}
