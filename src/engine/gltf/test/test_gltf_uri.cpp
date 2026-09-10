#include "gltf-uri.h"

#include <catch2/catch_test_macros.hpp>

using eng::gltf::decodeDataUri;
using eng::gltf::isDataUri;
using eng::gltf::uriToPath;

namespace {

/// @p text as the bytes it spells.
std::vector<uint8_t> bytesOf(std::string_view text) {
  return {text.begin(), text.end()};
}

}  // namespace

TEST_CASE("a base64 data URI decodes to its bytes") {
  REQUIRE(decodeDataUri("data:application/octet-stream;base64,SGVsbG8=") ==
          bytesOf("Hello"));
  REQUIRE(decodeDataUri("data:application/gltf-buffer;base64,SGk=") ==
          bytesOf("Hi"));
}

TEST_CASE("an empty payload is no bytes") {
  REQUIRE(decodeDataUri("data:application/octet-stream;base64,") ==
          std::vector<uint8_t>{});
}

TEST_CASE("a data URI that is not base64 is refused") {
  REQUIRE_FALSE(decodeDataUri("data:text/plain,Hello").has_value());
  REQUIRE_FALSE(decodeDataUri("data:x;base64,SG*s").has_value());
  REQUIRE_FALSE(decodeDataUri("arm.bin").has_value());
}

TEST_CASE("only a data: URI is inline") {
  REQUIRE(isDataUri("data:x;base64,AA=="));
  REQUIRE_FALSE(isDataUri("textures/data.png"));
}

TEST_CASE("percent escapes in a relative URI become the bytes they name") {
  REQUIRE(uriToPath("arm%20data.bin") == "arm data.bin");
  REQUIRE(uriToPath("a%2Fb") == "a/b");
}

TEST_CASE("a stray percent sign is kept as written") {
  REQUIRE(uriToPath("100%") == "100%");
  REQUIRE(uriToPath("50%zz.bin") == "50%zz.bin");
}
