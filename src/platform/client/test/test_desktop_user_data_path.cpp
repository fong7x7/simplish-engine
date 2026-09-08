#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-user-data-path.h>
#include <filesystem>

namespace fs = std::filesystem;
using namespace eng::client;

TEST_CASE("the user data path is absolute") {
  const fs::path path = desktopUserDataPath("SimplishTest", "PathCase");

  // A relative path would land wherever the process happened to be started
  // from, which is exactly what this exists to stop.
  REQUIRE_FALSE(path.empty());
  REQUIRE(path.is_absolute());
}

TEST_CASE("the user data directory exists once asked for") {
  const fs::path path = desktopUserDataPath("SimplishTest", "PathCase");

  // The caller writes a file into it straight away, so it has to be there.
  REQUIRE(fs::is_directory(path));
}

TEST_CASE("the path is not inside the build or source tree") {
  const fs::path path = desktopUserDataPath("SimplishTest", "PathCase");
  const fs::path here = fs::current_path();

  // The whole point: what the editor writes on every run must not land in
  // the checkout it was launched from.
  const std::string text = path.string();
  REQUIRE(text.find(here.string()) == std::string::npos);
}

TEST_CASE("different applications get different directories") {
  const fs::path editor = desktopUserDataPath("SimplishTest", "PathCase");
  const fs::path other = desktopUserDataPath("SimplishTest", "OtherCase");

  REQUIRE(editor != other);
}

TEST_CASE("asking twice gives the same directory") {
  // The path names where a list already written is to be found again, so it
  // has to be stable across runs, not just within one.
  REQUIRE(desktopUserDataPath("SimplishTest", "PathCase") ==
          desktopUserDataPath("SimplishTest", "PathCase"));
}
