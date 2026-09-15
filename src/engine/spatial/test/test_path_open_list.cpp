#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <engine/spatial/path-open-list.h>
#include <vector>

using eng::spatial::PATH_OPEN_SPAN;
using eng::spatial::PathOpenList;

namespace {

/// Every index left on @p list, in the order it gives them up.
std::vector<uint32_t> drain(PathOpenList& list) {
  std::vector<uint32_t> order;
  while (!list.empty()) {
    order.push_back(list.take());
  }
  return order;
}

}  // namespace

TEST_CASE("an open list gives up the smallest total first") {
  PathOpenList list;
  list.clear(100);
  list.push(120, 0, 1);
  list.push(100, 0, 2);
  list.push(114, 0, 3);
  REQUIRE(drain(list) == std::vector<uint32_t>{2, 3, 1});
}

TEST_CASE("an open list breaks a tie of totals by the smaller remainder, "
          "then the smaller index") {
  PathOpenList list;
  list.clear(50);
  list.push(50, 30, 7);
  list.push(50, 10, 9);
  list.push(50, 10, 4);
  REQUIRE(drain(list) == std::vector<uint32_t>{4, 9, 7});
}

TEST_CASE("an open list keeps order as totals climb round its buckets") {
  // Pushed as A* pushes them: each cell taken offers totals at most two
  // diagonal steps, 28, above its own. Each index is its total, so taking
  // them in order of total takes them in order of index.
  PathOpenList list;
  list.clear(0);
  list.push(0, 0, 0);
  std::vector<uint32_t> taken;
  while (taken.size() < 400) {
    const uint32_t total = list.take();
    taken.push_back(total);
    list.push(total + 28, 0, total + 28);
    list.push(total + 14, 0, total + 14);
  }
  REQUIRE(std::ranges::is_sorted(taken));
  REQUIRE(taken.back() > 3 * PATH_OPEN_SPAN);
}

TEST_CASE("a cleared open list holds nothing from before") {
  PathOpenList list;
  list.clear(10);
  list.push(12, 0, 1);
  list.clear(3);
  REQUIRE(list.empty());
  list.push(3, 0, 5);
  REQUIRE(drain(list) == std::vector<uint32_t>{5});
}
