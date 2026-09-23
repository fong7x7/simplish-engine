// Ogg Vorbis through stb_vorbis, decoded whole into 16-bit samples and
// widened to float. Declarations only here: the implementation is compiled
// once, in stb-vorbis-impl.cpp.

#include <cstdlib>
#include <engine/audio/audio-decode.h>
#include <limits>

#define STB_VORBIS_HEADER_ONLY
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
// stb_vorbis ships as one .c file; with STB_VORBIS_HEADER_ONLY it is a header.
// NOLINTNEXTLINE(bugprone-suspicious-include)
#include <stb_vorbis.c>
#pragma clang diagnostic pop

namespace eng::audio {

namespace {

  /// @p count interleaved 16-bit samples as floats, -1 to 1.
  std::vector<float> widen(const short* samples, size_t count) {
    std::vector<float> out(count);
    for (size_t i = 0; i < count; ++i) {
      out[i] = static_cast<float>(samples[i]) / 32768.0F;
    }
    return out;
  }

  /// A clip from what stb_vorbis decoded: @p frames frames of @p channels
  /// at @p rate. Nothing unless it is mono or stereo and not empty.
  std::optional<AudioClip> toClip(int frames, int channels, int rate,
                                  const short* samples) {
    if (frames <= 0 || (channels != 1 && channels != 2) || rate <= 0) {
      return std::nullopt;
    }
    return AudioClip{.samples =
                         widen(samples, static_cast<size_t>(frames) *
                                            static_cast<size_t>(channels)),
                     .sample_rate = static_cast<uint32_t>(rate),
                     .channels = static_cast<uint8_t>(channels)};
  }

}  // namespace

std::optional<AudioClip> decodeOgg(std::span<const std::byte> bytes) {
  if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return std::nullopt;
  }
  int channels = 0;
  int rate = 0;
  short* samples = nullptr;
  const int frames = stb_vorbis_decode_memory(
      reinterpret_cast<const unsigned char*>(bytes.data()),
      static_cast<int>(bytes.size()), &channels, &rate, &samples);
  std::optional<AudioClip> clip = toClip(frames, channels, rate, samples);
  // stb_vorbis allocated the samples with malloc; they are released the way
  // they were made, once copied out.
  // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,cppcoreguidelines-owning-memory)
  std::free(samples);
  return clip;
}

}  // namespace eng::audio
