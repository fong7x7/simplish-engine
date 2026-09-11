#pragma once

// Polyfill for std::expected / std::unexpected (C++23).
// AppleClang 14 ships libc++ without <expected>, and so does libstdc++ before
// GCC 12, which Clang uses on Linux. This header provides a minimal subset
// that covers all project uses until the toolchain catches up. When
// __cpp_lib_expected is defined the real <expected> is used instead.
//
// <version> comes first because it is what defines __cpp_lib_expected.
// Without it the choice would depend on what the including file happened to
// include before this header, and two files that chose differently would
// define std::expected two ways — an ODR violation that crashes rather than
// fails to link. test_expected_polyfill.cpp includes this header first to
// hold that line.
#include <version>

#if __has_include(<expected>) && defined(__cpp_lib_expected)
#include <expected>

namespace eng {
/// True: std::expected here is the standard library's own.
inline constexpr bool USES_STD_EXPECTED = true;
}  // namespace eng

#else

#include <type_traits>
#include <utility>
#include <variant>

// NOLINTBEGIN(bugprone-std-namespace-modification,cert-dcl58-cpp,readability-identifier-naming,hicpp-explicit-conversions,google-explicit-constructor)
namespace std {

/// Tag type wrapping an error value, mirroring C++23 std::unexpected.
template <typename E>
class unexpected {  // NOLINT(one-type-per-file) polyfill components for
                    // std::expected
public:
  constexpr explicit unexpected(const E& e) : error_(e) {}
  constexpr explicit unexpected(E&& e) : error_(std::move(e)) {}
  constexpr const E& error() const& { return error_; }
  constexpr E& error() & { return error_; }
  constexpr E&& error() && { return std::move(error_); }

private:
  /// Stored error value.
  E error_;
};

template <typename E> unexpected(E) -> unexpected<E>;

/// Minimal std::expected polyfill for non-void value types.
template <typename T, typename E>
class expected {  // NOLINT(one-type-per-file) polyfill components for
                  // std::expected
public:
  constexpr expected() : data_(T{}) {}
  constexpr expected(const T& val) : data_(val) {}
  constexpr expected(T&& val) : data_(std::move(val)) {}
  constexpr expected(const unexpected<E>& u) : data_(u.error()) {}
  constexpr expected(unexpected<E>&& u) : data_(std::move(u).error()) {}

  constexpr bool has_value() const { return std::holds_alternative<T>(data_); }
  constexpr explicit operator bool() const { return has_value(); }

  constexpr T& value() & { return std::get<T>(data_); }
  constexpr const T& value() const& { return std::get<T>(data_); }
  constexpr T&& value() && { return std::get<T>(std::move(data_)); }

  constexpr T& operator*() & { return std::get<T>(data_); }
  constexpr const T& operator*() const& { return std::get<T>(data_); }
  constexpr T&& operator*() && { return std::get<T>(std::move(data_)); }

  constexpr T* operator->() { return &std::get<T>(data_); }
  constexpr const T* operator->() const { return &std::get<T>(data_); }

  constexpr E& error() & { return std::get<E>(data_); }
  constexpr const E& error() const& { return std::get<E>(data_); }

private:
  /// Stores either the value or the error.
  std::variant<T, E> data_;
};

/// Specialization for expected<void, E>.
template <typename E>
class expected<void, E> {  // NOLINT(one-type-per-file) polyfill components for
                           // std::expected
public:
  constexpr expected() = default;
  constexpr expected(const unexpected<E>& u)
    : error_(u.error()), has_val_(false) {}
  constexpr expected(unexpected<E>&& u)
    : error_(std::move(u).error()), has_val_(false) {}

  constexpr bool has_value() const { return has_val_; }
  constexpr explicit operator bool() const { return has_val_; }

  constexpr E& error() & { return error_; }
  constexpr const E& error() const& { return error_; }

private:
  /// Stored error (default-constructed when has_value is true).
  E error_{};
  /// Whether this expected holds a value (void) or an error.
  bool has_val_ = true;
};

}  // namespace std
// NOLINTEND(bugprone-std-namespace-modification,cert-dcl58-cpp,readability-identifier-naming,hicpp-explicit-conversions,google-explicit-constructor)

namespace eng {
/// False: std::expected here is the polyfill above.
inline constexpr bool USES_STD_EXPECTED = false;
}  // namespace eng

#endif
