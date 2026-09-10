#include <catch2/catch_test_macros.hpp>
#include <engine/render-mesh/skin-palette.h>
#include <vector>

using eng::makeSkinPalette;
using eng::Mat4;
using eng::MESH_MAX_SKIN_JOINTS;
using eng::SkinPalette;

namespace {

/// Whether joint @p joint's rows in @p palette are the identity's.
bool isIdentityJoint(const SkinPalette& palette, size_t joint) {
  for (size_t row = 0; row < 3; ++row) {
    for (size_t column = 0; column < 4; ++column) {
      const float expected = row == column ? 1.0f : 0.0f;
      if (palette.rows[joint * 3 + row][column] != expected) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

TEST_CASE("a palette holds each matrix's top three rows") {
  Mat4 m = Mat4::identity();
  m(0, 3) = 5.0f;  // translate X
  m(1, 0) = 2.0f;  // shear Y by X
  const std::vector<Mat4> skin{Mat4::identity(), m};
  const SkinPalette palette = makeSkinPalette(skin);
  REQUIRE(isIdentityJoint(palette, 0));
  REQUIRE(palette.rows[3][3] == 5.0f);
  REQUIRE(palette.rows[4][0] == 2.0f);
  REQUIRE(palette.rows[5][2] == 1.0f);
}

TEST_CASE("palette slots past the skin are the identity") {
  Mat4 m = Mat4::identity();
  m(2, 3) = 1.0f;
  const std::vector<Mat4> skin{m};
  const SkinPalette palette = makeSkinPalette(skin);
  REQUIRE_FALSE(isIdentityJoint(palette, 0));
  REQUIRE(isIdentityJoint(palette, 1));
  REQUIRE(isIdentityJoint(palette, MESH_MAX_SKIN_JOINTS - 1));
}

TEST_CASE("an empty skin is a palette of identities") {
  const SkinPalette palette = makeSkinPalette({});
  REQUIRE(isIdentityJoint(palette, 0));
}

TEST_CASE("matrices past the palette's capacity are dropped") {
  std::vector<Mat4> skin(MESH_MAX_SKIN_JOINTS + 4, Mat4::identity());
  skin.back()(0, 3) = 9.0f;
  const SkinPalette palette = makeSkinPalette(skin);
  REQUIRE(isIdentityJoint(palette, MESH_MAX_SKIN_JOINTS - 1));
}
