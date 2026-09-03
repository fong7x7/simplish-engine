#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-dialog-start-folder.h>

using namespace eng::client;

// Req: docs/editor/REQUIREMENTS.md §4.1 — New Project opens where a person
//      keeps their work

TEST_CASE("documents wins when it exists", "[platform][client][dialog]") {
  REQUIRE(chooseDialogStartFolder("/Users/a/Documents", "/Users/a") ==
          "/Users/a/Documents");
}

TEST_CASE("home is the fallback", "[platform][client][dialog]") {
  // A stripped container or a locked-down account may have no Documents.
  REQUIRE(chooseDialogStartFolder(nullptr, "/Users/a") == "/Users/a");
  REQUIRE(chooseDialogStartFolder("", "/Users/a") == "/Users/a");
}

TEST_CASE("neither folder leaves the choice to the OS",
          "[platform][client][dialog]") {
  // Empty means "no default location", which is what SDL takes a null for.
  REQUIRE(chooseDialogStartFolder(nullptr, nullptr).empty());
  REQUIRE(chooseDialogStartFolder("", "").empty());
}
