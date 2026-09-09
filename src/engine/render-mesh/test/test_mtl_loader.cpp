#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/mtl-loader.h>

using namespace eng;

TEST_CASE("a material's diffuse map is read") {
  const auto map =
      parseMtlDiffuseMap("newmtl crate\nmap_Kd crate.png\n", "crate");
  REQUIRE(map.has_value());
  REQUIRE(*map == "crate.png");
}

TEST_CASE("the right material's map is read when there are several") {
  constexpr const char* MTL = R"(
newmtl body
map_Kd body.png
newmtl glass
map_Kd glass.png
)";
  REQUIRE(*parseMtlDiffuseMap(MTL, "glass") == "glass.png");
  REQUIRE(*parseMtlDiffuseMap(MTL, "body") == "body.png");
}

TEST_CASE("no material named takes the first map there is") {
  // A single-material export often never writes `usemtl`, and its one map
  // is unambiguously the one to use.
  REQUIRE(*parseMtlDiffuseMap("newmtl only\nmap_Kd only.png\n", "") ==
          "only.png");
}

TEST_CASE("a material that names no map reports none") {
  REQUIRE_FALSE(
      parseMtlDiffuseMap("newmtl flat\nKd 1 1 1\n", "flat").has_value());
}

TEST_CASE("a material that is not there reports none") {
  REQUIRE_FALSE(parseMtlDiffuseMap("newmtl body\nmap_Kd body.png\n", "glass")
                    .has_value());
}

TEST_CASE("comments and blank lines are skipped") {
  REQUIRE(*parseMtlDiffuseMap("# a comment\n\nnewmtl m\n  map_Kd  m.png  \n",
                              "m") == "m.png");
}

TEST_CASE("a map with options keeps only its path") {
  // `map_Kd` may carry flags before the file, and every flag takes
  // arguments, so what is left at the end is the image.
  REQUIRE(*parseMtlDiffuseMap("newmtl m\nmap_Kd -s 1 1 1 wood.png\n", "m") ==
          "wood.png");
}

TEST_CASE("a path with spaces in it survives") {
  // No options, so nothing is split off: the whole value is the file.
  REQUIRE(*parseMtlDiffuseMap("newmtl m\nmap_Kd my texture.png\n", "m") ==
          "my texture.png");
}

TEST_CASE("carriage returns are not part of the path") {
  REQUIRE(*parseMtlDiffuseMap("newmtl m\r\nmap_Kd m.png\r\n", "m") == "m.png");
}

TEST_CASE("an empty library reports none") {
  REQUIRE_FALSE(parseMtlDiffuseMap("", "").has_value());
}
