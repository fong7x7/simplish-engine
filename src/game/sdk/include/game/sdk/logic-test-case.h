#pragma once

/// @file logic-test-case.h
/// @brief One of a project's logic tests, as its library exports it.
/// @par Threading
/// A value type.

#include <cstdint>
#include <game/sdk/logic-test.h>

namespace eng::game::sdk {

/// A test `SIMPLISH_LOGIC_TEST` registered: plain enough to cross from a
/// project's library to the program running it.
struct LogicTestCase {
  /// What it is called.
  const char* name = "";
  /// The level it plays, by id.
  const char* level = "main";
  /// Players in its run, 1 to 4.
  uint8_t players = 1;
  /// Its body.
  void (*body)(LogicTest&) = nullptr;
};

/// Signature of the function a logic library exports its tests by.
using LogicTestsFn = const LogicTestCase* (*)(uint32_t* count);

}  // namespace eng::game::sdk
