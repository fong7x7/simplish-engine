#include <game/logic/game-logic-instance.h>
#include <utility>

namespace eng::game {

GameLogicInstance::GameLogicInstance(GameLogicFactory factory) {
  if (factory.create == nullptr || factory.destroy == nullptr) {
    return;
  }
  logic_ = factory.create();
  destroy_ = factory.destroy;
}

GameLogicInstance::~GameLogicInstance() {
  reset();
}

GameLogicInstance::GameLogicInstance(GameLogicInstance&& other) noexcept
  : logic_(std::exchange(other.logic_, nullptr)),
    destroy_(std::exchange(other.destroy_, nullptr)) {}

GameLogicInstance&
GameLogicInstance::operator=(GameLogicInstance&& other) noexcept {
  if (this != &other) {
    reset();
    logic_ = std::exchange(other.logic_, nullptr);
    destroy_ = std::exchange(other.destroy_, nullptr);
  }
  return *this;
}

void GameLogicInstance::reset() {
  if (logic_ != nullptr && destroy_ != nullptr) {
    destroy_(logic_);
  }
  logic_ = nullptr;
  destroy_ = nullptr;
}

}  // namespace eng::game
