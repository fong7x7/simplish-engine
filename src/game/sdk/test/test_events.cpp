#include "support/list-hash.h"
#include "support/sdk-rig.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <game/sdk/events.h>
#include <vector>

using namespace eng::game;
using namespace eng::game::sdk;
using eng::game::sdk::test::ListHash;

namespace {

/// The logic's own event, in these tests.
struct Rang {
  /// Which bell.
  uint32_t bell = 0;
};

/// A queue of `Rang` whose two handlers write down, in order, what each
/// heard — and the first of which rings bell 2 whenever it hears bell 1.
struct Belfry {
  /// The queue.
  Events<Rang> rang;
  /// What the handlers heard: 10 + bell for the first, 20 + bell for the
  /// second.
  std::vector<uint32_t> heard;

  Belfry() {
    rang.subscribe([this](GameLogicWorld&, const Rang& e) {
      heard.push_back(10 + e.bell);
      if (e.bell == 1) {
        rang.emit({2});
      }
    });
    rang.subscribe([this](GameLogicWorld&, const Rang& e) {
      heard.push_back(20 + e.bell);
    });
  }
};

}  // namespace

TEST_CASE("events are heard at dispatch, by every handler, in order") {
  Belfry belfry;
  test::ScriptedLogic logic([&](GameLogicWorld& world) {
    belfry.rang.emit({1});
    CHECK(belfry.heard.empty());
    belfry.rang.dispatch(world);
  });

  test::runLogic(logic, 1, test::sdkArena(), {});

  // Bell 2, emitted while bell 1 was heard, is heard in the same dispatch.
  CHECK(belfry.heard == std::vector<uint32_t>{11, 21, 12, 22});
  CHECK(belfry.rang.pending() == 0);
}

TEST_CASE("events not yet heard are hashed; heard ones are gone") {
  Events<Rang> rang;
  ListHash empty;
  ListHash waiting;

  rang.hashInto(empty);
  rang.emit({7});
  rang.hashInto(waiting);

  CHECK(waiting.added.size() > empty.added.size());
}
