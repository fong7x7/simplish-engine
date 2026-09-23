#include <catch2/catch_test_macros.hpp>
#include <engine/audio/audio-command-queue.h>
#include <thread>

using eng::audio::AudioCommand;
using eng::audio::AudioCommandKind;
using eng::audio::AudioCommandQueue;

namespace {
AudioCommand gainOf(float gain) {
  return {.kind = AudioCommandKind::MASTER_GAIN, .gain = gain};
}
}  // namespace

TEST_CASE("capacity rounds up to a power of two") {
  REQUIRE(AudioCommandQueue(5).capacity() == 8);
  REQUIRE(AudioCommandQueue(0).capacity() == 2);
}

TEST_CASE("commands come out in the order they went in") {
  AudioCommandQueue queue(4);
  REQUIRE_FALSE(queue.pop().has_value());
  REQUIRE(queue.push(gainOf(1.0F)));
  REQUIRE(queue.push(gainOf(2.0F)));
  REQUIRE(queue.pop()->gain == 1.0F);
  REQUIRE(queue.pop()->gain == 2.0F);
  REQUIRE_FALSE(queue.pop().has_value());
}

TEST_CASE("a full queue refuses, and takes more once drained") {
  AudioCommandQueue queue(4);
  for (int i = 0; i < 4; ++i) {
    REQUIRE(queue.push(gainOf(static_cast<float>(i))));
  }
  REQUIRE_FALSE(queue.push(gainOf(9.0F)));
  REQUIRE(queue.pop()->gain == 0.0F);
  REQUIRE(queue.push(gainOf(4.0F)));
  for (int i = 1; i <= 4; ++i) {
    REQUIRE(queue.pop()->gain == static_cast<float>(i));
  }
}

TEST_CASE("one thread pushes while another pops, and nothing is lost") {
  AudioCommandQueue queue(16);
  constexpr int COUNT = 20000;
  std::thread producer([&queue] {
    for (int i = 0; i < COUNT;) {
      i += queue.push(gainOf(static_cast<float>(i))) ? 1 : 0;
    }
  });
  int expected = 0;
  bool in_order = true;
  while (expected < COUNT) {
    const auto command = queue.pop();
    in_order = in_order &&
               (!command || command->gain == static_cast<float>(expected++));
  }
  producer.join();
  REQUIRE(in_order);
}
