#include <array>
#include <game/content/step-set-stride.h>

namespace eng::game {

namespace {

  /// Every step set's stride, by `StepSet`: default, boots, bare, claws,
  /// heavy.
  constexpr std::array<float, STEP_SET_COUNT> STRIDES{0.75F, 0.8F, 0.7F, 0.45F,
                                                      1.1F};

}  // namespace

float stepSetStride(StepSet steps) {
  return STRIDES[static_cast<size_t>(steps)];
}

}  // namespace eng::game
