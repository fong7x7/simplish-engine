// RIFF WAV, read chunk by chunk: the format from "fmt ", the samples from
// "data", everything else skipped. Integer PCM of 8 to 32 bits and 32-bit
// float, plain or in a WAVE_FORMAT_EXTENSIBLE wrapper.

#include <cstring>
#include <engine/audio/audio-decode.h>
#include <fstream>
#include <iterator>
#include <string_view>

namespace eng::audio {

namespace {

  /// The format tag of integer PCM.
  constexpr uint16_t WAV_PCM = 1;
  /// The format tag of IEEE float.
  constexpr uint16_t WAV_FLOAT = 3;
  /// The format tag that says the real one is in the extension.
  constexpr uint16_t WAV_EXTENSIBLE = 0xFFFE;

  /// What a "fmt " chunk says the samples are.
  struct WavFormat {
    /// PCM or float, the extension unwrapped.
    uint16_t tag = 0;
    /// Channels a frame.
    uint16_t channels = 0;
    /// Frames a second.
    uint32_t rate = 0;
    /// Bits a sample.
    uint16_t bits = 0;
  };

  /// The little-endian number of @p size bytes at @p at in @p bytes; zero
  /// past the end.
  uint32_t readLe(std::span<const std::byte> bytes, size_t at, size_t size) {
    uint32_t value = 0;
    for (size_t i = 0; i < size && at + i < bytes.size(); ++i) {
      value |= std::to_integer<uint32_t>(bytes[at + i]) << (8U * i);
    }
    return value;
  }

  /// Whether the four bytes at @p at spell @p tag.
  bool isTag(std::span<const std::byte> bytes, size_t at,
             std::string_view tag) {
    return at + 4 <= bytes.size() &&
           std::memcmp(bytes.data() + at, tag.data(), 4) == 0;
  }

  /// The format in the "fmt " chunk @p chunk.
  WavFormat readFormat(std::span<const std::byte> chunk) {
    WavFormat format{.tag = static_cast<uint16_t>(readLe(chunk, 0, 2)),
                     .channels = static_cast<uint16_t>(readLe(chunk, 2, 2)),
                     .rate = readLe(chunk, 4, 4),
                     .bits = static_cast<uint16_t>(readLe(chunk, 14, 2))};
    if (format.tag == WAV_EXTENSIBLE) {
      format.tag = static_cast<uint16_t>(readLe(chunk, 24, 2));
    }
    return format;
  }

  /// Whether a clip can be made from samples in @p format.
  bool supported(const WavFormat& format) {
    const bool pcm =
        format.tag == WAV_PCM && (format.bits == 8 || format.bits == 16 ||
                                  format.bits == 24 || format.bits == 32);
    const bool real = format.tag == WAV_FLOAT && format.bits == 32;
    return (pcm || real) && (format.channels == 1 || format.channels == 2) &&
           format.rate > 0;
  }

  /// The sample of @p bits at @p at, -1 to 1.
  float readSample(std::span<const std::byte> data, size_t at,
                   const WavFormat& format) {
    const uint32_t raw = readLe(data, at, format.bits / 8U);
    if (format.tag == WAV_FLOAT) {
      float value = 0.0F;
      std::memcpy(&value, &raw, sizeof(value));
      return value;
    }
    if (format.bits == 8) {
      return (static_cast<float>(raw) - 128.0F) / 128.0F;
    }
    const uint32_t shift = 32U - format.bits;
    const auto value = static_cast<int32_t>(raw << shift);
    return static_cast<float>(value) / 2147483648.0F;
  }

  /// The samples in @p data, read as @p format says.
  AudioClip readClip(std::span<const std::byte> data, const WavFormat& format) {
    const size_t width = format.bits / 8U;
    AudioClip clip{.sample_rate = format.rate,
                   .channels = static_cast<uint8_t>(format.channels)};
    const size_t frames = data.size() / (width * format.channels);
    clip.samples.resize(frames * format.channels);
    for (size_t i = 0; i < clip.samples.size(); ++i) {
      clip.samples[i] = readSample(data, i * width, format);
    }
    return clip;
  }

  /// The two chunks a clip needs, found in a RIFF file's body.
  struct WavChunks {
    /// The "fmt " chunk's body; empty if there was none.
    std::span<const std::byte> format{};
    /// The "data" chunk's body; empty if there was none.
    std::span<const std::byte> data{};
  };

  /// Walk @p bytes' chunks from the first after the RIFF header.
  WavChunks findChunks(std::span<const std::byte> bytes) {
    WavChunks chunks;
    size_t at = 12;
    while (at + 8 <= bytes.size()) {
      const size_t size = readLe(bytes, at + 4, 4);
      const size_t body = std::min(size, bytes.size() - at - 8);
      if (isTag(bytes, at, "fmt ")) {
        chunks.format = bytes.subspan(at + 8, body);
      } else if (isTag(bytes, at, "data")) {
        chunks.data = bytes.subspan(at + 8, body);
      }
      at += 8 + size + (size & 1U);
    }
    return chunks;
  }

}  // namespace

std::optional<AudioClip> decodeWav(std::span<const std::byte> bytes) {
  if (!isTag(bytes, 0, "RIFF") || !isTag(bytes, 8, "WAVE")) {
    return std::nullopt;
  }
  const WavChunks chunks = findChunks(bytes);
  if (chunks.format.size() < 16 || chunks.data.empty()) {
    return std::nullopt;
  }
  const WavFormat format = readFormat(chunks.format);
  if (!supported(format)) {
    return std::nullopt;
  }
  return readClip(chunks.data, format);
}

std::optional<AudioClip> decodeAudio(std::span<const std::byte> bytes) {
  if (isTag(bytes, 0, "RIFF")) {
    return decodeWav(bytes);
  }
  if (isTag(bytes, 0, "OggS")) {
    return decodeOgg(bytes);
  }
  return std::nullopt;
}

std::optional<AudioClip> loadAudioFile(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return std::nullopt;
  }
  const std::vector<char> raw{std::istreambuf_iterator<char>(in),
                              std::istreambuf_iterator<char>()};
  return decodeAudio(std::as_bytes(std::span(raw)));
}

}  // namespace eng::audio
