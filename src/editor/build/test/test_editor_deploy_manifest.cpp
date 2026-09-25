#include <catch2/catch_test_macros.hpp>
#include <editor/build/editor-deploy-manifest.h>

using namespace eng::editor;

TEST_CASE("a deploy manifest reads back as it was written") {
  const EditorDeployManifest written{"Dig", {"main", "caves"}, "caves", true};

  const auto read = parseDeployManifest(serializeDeployManifest(written));

  REQUIRE(read.has_value());
  REQUIRE(read->name == "Dig");
  REQUIRE(read->levels == std::vector<std::string>{"main", "caves"});
  REQUIRE(read->start_level == "caves");
  REQUIRE(read->has_logic);
}

TEST_CASE("text that is not a manifest reads as nothing") {
  REQUIRE_FALSE(parseDeployManifest("[]").has_value());
  REQUIRE_FALSE(parseDeployManifest(R"({"schema": 4})").has_value());
}
