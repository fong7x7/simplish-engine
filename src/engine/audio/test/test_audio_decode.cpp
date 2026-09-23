#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <engine/audio/audio-decode.h>
#include <filesystem>
#include <fstream>
#include <string>

using eng::audio::decodeAudio;
using eng::audio::decodeOgg;
using eng::audio::decodeWav;
using eng::audio::loadAudioFile;

namespace {

/// A WAV file written by hand, a byte at a time.
struct WavWriter {
  std::vector<std::byte> bytes;

  void tag(const char* four) {
    for (int i = 0; i < 4; ++i) {
      bytes.push_back(static_cast<std::byte>(four[i]));
    }
  }
  void le(uint32_t value, int size) {
    for (int i = 0; i < size; ++i) {
      bytes.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
  }
};

/// Whether a WAV's format is plain or in the extensible wrapper.
enum class WavWrapping : uint8_t { PLAIN, EXTENSIBLE };

/// What a hand-written WAV holds.
struct WavSpec {
  /// The format tag: 1 PCM, 3 float, anything else unsupported.
  uint16_t tag = 1;
  /// Channels a frame.
  uint16_t channels = 1;
  /// Bits a sample.
  uint16_t bits = 16;
  /// The samples' bytes.
  std::vector<std::byte> data;
  /// Plain or extensible.
  WavWrapping wrapping = WavWrapping::PLAIN;
};

/// The "fmt " chunk of @p spec, at 48 kHz.
void writeFormat(WavWriter& w, const WavSpec& spec) {
  const bool extensible = spec.wrapping == WavWrapping::EXTENSIBLE;
  w.tag("fmt ");
  w.le(extensible ? 40 : 16, 4);
  w.le(extensible ? 0xFFFE : spec.tag, 2);
  w.le(spec.channels, 2);
  w.le(48000, 4);
  w.le(48000U * spec.channels * spec.bits / 8, 4);
  w.le(spec.channels * spec.bits / 8U, 2);
  w.le(spec.bits, 2);
  if (extensible) {
    w.le(22, 2);
    w.le(spec.bits, 2);
    w.le(0, 4);
    w.le(spec.tag, 2);
    w.bytes.resize(w.bytes.size() + 14);
  }
}

/// A whole WAV of @p spec, with an unknown chunk of odd size before its
/// data for the decoder to skip.
std::vector<std::byte> makeWav(const WavSpec& spec) {
  WavWriter w;
  w.tag("RIFF");
  w.le(0, 4);  // the RIFF size, patched below
  w.tag("WAVE");
  writeFormat(w, spec);
  w.tag("LIST");
  w.le(3, 4);  // odd size: padded to four
  w.le(0, 4);
  w.tag("data");
  w.le(static_cast<uint32_t>(spec.data.size()), 4);
  w.bytes.insert(w.bytes.end(), spec.data.begin(), spec.data.end());
  const auto riff = static_cast<uint32_t>(w.bytes.size() - 8);
  std::memcpy(w.bytes.data() + 4, &riff, sizeof(riff));
  return w.bytes;
}

std::vector<std::byte> int16s(std::initializer_list<int16_t> values) {
  WavWriter w;
  for (const int16_t v : values) {
    w.le(static_cast<uint16_t>(v), 2);
  }
  return w.bytes;
}

}  // namespace

TEST_CASE("16-bit mono PCM decodes to floats") {
  const auto clip = decodeWav(makeWav({1, 1, 16, int16s({0, 16384, -32768})}));
  REQUIRE(clip.has_value());
  REQUIRE(clip->channels == 1);
  REQUIRE(clip->sample_rate == 48000);
  REQUIRE(clip->samples == std::vector<float>{0.0F, 0.5F, -1.0F});
}

TEST_CASE("stereo stays interleaved") {
  const auto clip = decodeWav(makeWav({1, 2, 16, int16s({16384, -16384})}));
  REQUIRE(clip->channels == 2);
  REQUIRE(clip->samples == std::vector<float>{0.5F, -0.5F});
}

TEST_CASE("8-bit is unsigned around 128") {
  const auto clip = decodeWav(
      makeWav({1, 1, 8, {std::byte{128}, std::byte{0}, std::byte{192}}}));
  REQUIRE(clip->samples == std::vector<float>{0.0F, -1.0F, 0.5F});
}

TEST_CASE("24-bit is sign-extended") {
  const auto clip = decodeWav(
      makeWav({1,
               1,
               24,
               {std::byte{0}, std::byte{0}, std::byte{0xC0},      // -0.5
                std::byte{0}, std::byte{0}, std::byte{0x40}}}));  // 0.5
  REQUIRE(clip->samples == std::vector<float>{-0.5F, 0.5F});
}

TEST_CASE("float and extensible-wrapped float decode as written") {
  std::vector<std::byte> data(8);
  const float values[2] = {0.25F, -0.75F};
  std::memcpy(data.data(), values, sizeof(values));
  REQUIRE(decodeWav(makeWav({3, 1, 32, data}))->samples ==
          std::vector<float>{0.25F, -0.75F});
  REQUIRE(
      decodeWav(makeWav({3, 1, 32, data, WavWrapping::EXTENSIBLE}))->samples ==
      std::vector<float>{0.25F, -0.75F});
}

TEST_CASE("what cannot be played is nothing") {
  // Compressed (ADPCM), five channels, not a RIFF, and cut short.
  REQUIRE_FALSE(decodeWav(makeWav({2, 1, 4, int16s({1})})).has_value());
  REQUIRE_FALSE(decodeWav(makeWav({1, 5, 16, int16s({1, 2, 3, 4, 5})})));
  REQUIRE_FALSE(decodeWav(int16s({1, 2, 3, 4, 5, 6, 7, 8})).has_value());
  auto cut = makeWav({1, 1, 16, int16s({1})});
  cut.resize(20);
  REQUIRE_FALSE(decodeWav(cut).has_value());
}

TEST_CASE("Ogg that is not Vorbis is nothing") {
  WavWriter w;
  w.tag("OggS");
  for (int i = 0; i < 64; ++i) {
    w.le(0xA5, 1);
  }
  REQUIRE_FALSE(decodeOgg(w.bytes).has_value());
  REQUIRE_FALSE(decodeAudio(w.bytes).has_value());
}

TEST_CASE("decodeAudio tells the formats apart by their bytes") {
  const auto wav = makeWav({1, 1, 16, int16s({16384})});
  REQUIRE(decodeAudio(wav)->samples == std::vector<float>{0.5F});
  REQUIRE_FALSE(decodeAudio(int16s({1, 2, 3, 4})).has_value());
  REQUIRE_FALSE(decodeAudio({}).has_value());
}

TEST_CASE("a clip loads from a file, and a missing file is nothing") {
  const auto path =
      std::filesystem::temp_directory_path() / "simplish-test-audio-decode.wav";
  const auto wav = makeWav({1, 1, 16, int16s({-16384, 16384})});
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(wav.data()),
              static_cast<std::streamsize>(wav.size()));
  }
  REQUIRE(loadAudioFile(path)->samples == std::vector<float>{-0.5F, 0.5F});
  std::filesystem::remove(path);
  REQUIRE_FALSE(loadAudioFile(path).has_value());
}
