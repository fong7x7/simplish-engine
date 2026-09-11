// The polyfill header comes first on purpose: nothing before it may define
// __cpp_lib_expected. That is the case in which it once chose its own
// std::expected while a file that included <string> first chose the standard
// library's — one type defined two ways. clang-format would sort Catch2, which
// pulls in <version>, above it.
// clang-format off
#include <engine/core/expected-polyfill.h>
#include <version>
// clang-format on

#include <catch2/catch_test_macros.hpp>
#include <string>

// Whether the standard library offers std::expected, asked after <version>,
// which is how every file that includes a standard header first sees it.
#if defined(__cpp_lib_expected)
constexpr bool LIBRARY_HAS_EXPECTED = true;
#else
constexpr bool LIBRARY_HAS_EXPECTED = false;
#endif

static_assert(eng::USES_STD_EXPECTED == LIBRARY_HAS_EXPECTED,
              "expected-polyfill.h must choose the same std::expected "
              "whatever was included before it");

namespace {
std::expected<int, std::string> half(int n) {
  if (n % 2 != 0) {
    return std::unexpected(std::string("odd"));
  }
  return n / 2;
}
}  // namespace

TEST_CASE("expected carries a value, or the error that replaced it") {
  const std::expected<int, std::string> even = half(8);
  REQUIRE(even.has_value());
  CHECK(*even == 4);

  const std::expected<int, std::string> odd = half(3);
  REQUIRE_FALSE(odd.has_value());
  CHECK(odd.error() == "odd");
}
