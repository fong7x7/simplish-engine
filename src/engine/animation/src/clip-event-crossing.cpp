#include <cmath>
#include <engine/animation/clip-event-crossing.h>

namespace eng::animation {

namespace {

  /// Append to @p crossed every index of @p times whose time, @p offset
  /// later, falls in @p window.
  void appendWithin(std::span<const float> times, double offset,
                    ClipWindow window, std::vector<size_t>& crossed) {
    for (size_t i = 0; i < times.size(); ++i) {
      const double t = times[i] + offset;
      if (window.from < t && t <= window.to) {
        crossed.push_back(i);
      }
    }
  }

}  // namespace

void crossedClipTimes(std::span<const float> times, float duration,
                      ClipWindow window, std::vector<size_t>& crossed) {
  const double span = window.to - window.from;
  if (duration <= 0.0F || span <= 0.0) {
    return;
  }
  const double length = duration;
  if (span >= length) {
    appendWithin(times, 0.0, {-1.0, length}, crossed);
    return;
  }
  // The window moved into the clip's first loop, where the times are; its
  // end may run on into the next loop, whose times come after this one's.
  const double a = window.from - std::floor(window.from / length) * length;
  appendWithin(times, 0.0, {a, a + span}, crossed);
  appendWithin(times, length, {a, a + span}, crossed);
}

}  // namespace eng::animation
