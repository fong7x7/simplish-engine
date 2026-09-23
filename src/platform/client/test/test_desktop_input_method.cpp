#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>
#include <engine/client/desktop-game-client.h>
#include <vector>

using eng::input::InputMethod;

namespace {

/// A client that is never opened, fed events directly, recording each
/// change of input method it is told about.
class MethodRecorder final : public eng::client::DesktopGameClient {
public:
  using DesktopGameClient::inputMethod;
  using DesktopGameClient::onEvent;

  /// Every change raised, in order.
  std::vector<InputMethod> changes;

protected:
  bool onTick(float /*dt*/) override { return false; }
  void onClientInputMethodChanged(InputMethod method) override {
    changes.push_back(method);
  }
};

/// An event of @p type and nothing else.
SDL_Event eventOf(SDL_EventType type) {
  SDL_Event event{};
  event.type = type;
  return event;
}

}  // namespace

TEST_CASE("the input method follows the device last used") {
  MethodRecorder client;
  REQUIRE(client.inputMethod() == InputMethod::POINTER);

  client.onEvent(eventOf(SDL_EVENT_MOUSE_MOTION));
  REQUIRE(client.changes.empty());

  client.onEvent(eventOf(SDL_EVENT_KEY_DOWN));
  client.onEvent(eventOf(SDL_EVENT_KEY_DOWN));
  REQUIRE(client.inputMethod() == InputMethod::KEYBOARD);

  // A wheel rather than a click: a click also syncs text input with the
  // GUI, which a client that was never opened does not have.
  client.onEvent(eventOf(SDL_EVENT_MOUSE_WHEEL));
  REQUIRE(client.changes ==
          std::vector{InputMethod::KEYBOARD, InputMethod::POINTER});
}
