#pragma once

/// @file audio-decode.h
/// @brief Turning WAV and Ogg Vorbis files into clips.
/// @par Threading
/// Pure functions, and a file read. Any thread.

#include <cstddef>
#include <engine/audio/audio-clip.h>
#include <filesystem>
#include <optional>
#include <span>

namespace eng::audio {

/// A clip from the bytes of a RIFF WAV: 8-, 16-, 24- or 32-bit integer PCM,
/// or 32-bit float, mono or stereo. Anything else — compressed WAVs, more
/// than two channels, a truncated file — is nothing.
[[nodiscard]] std::optional<AudioClip>
decodeWav(std::span<const std::byte> bytes);

/// A clip from the bytes of an Ogg Vorbis file, mono or stereo, decoded
/// whole by stb_vorbis. Anything it cannot read is nothing.
[[nodiscard]] std::optional<AudioClip>
decodeOgg(std::span<const std::byte> bytes);

/// A clip from @p bytes, whichever of the two they are, told apart by their
/// first four bytes rather than a file name.
[[nodiscard]] std::optional<AudioClip>
decodeAudio(std::span<const std::byte> bytes);

/// A clip from the file at @p path; nothing if it cannot be read or decoded.
[[nodiscard]] std::optional<AudioClip>
loadAudioFile(const std::filesystem::path& path);

}  // namespace eng::audio
