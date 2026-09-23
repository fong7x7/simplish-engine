// The desktop's audio backend: an SDL3 audio stream on the default playback
// device, in the engine's own format — 32-bit float, stereo, at its rate —
// so SDL converts nothing unless the hardware needs it. SDL asks for samples
// from its audio thread; the engine mixes them there.

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <engine/audio/audio-device.h>

namespace eng::audio {

namespace {

  /// Frames mixed at a time inside one callback.
  constexpr size_t CHUNK_FRAMES = 512;
  // SDL's device-id macros are C casts.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
  /// Whichever device the system plays through, following it if it changes.
  constexpr SDL_AudioDeviceID DEFAULT_PLAYBACK =
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
#pragma clang diagnostic pop

  /// The bytes in one stereo float frame.
  constexpr int FRAME_BYTES = 2 * static_cast<int>(sizeof(float));

  /// SDL's callback: mix at least @p additional bytes from the engine in
  /// @p userdata and hand them to @p stream.
  void SDLCALL feed(void* userdata, SDL_AudioStream* stream, int additional,
                    int /*total*/) {
    auto& engine = *static_cast<AudioEngine*>(userdata);
    std::array<float, CHUNK_FRAMES * 2> buffer{};
    auto owed =
        static_cast<size_t>((additional + FRAME_BYTES - 1) / FRAME_BYTES);
    while (owed > 0) {
      const size_t frames = std::min(owed, CHUNK_FRAMES);
      const std::span<float> chunk(buffer.data(), frames * 2);
      engine.render(chunk);
      SDL_PutAudioStreamData(stream, chunk.data(),
                             static_cast<int>(chunk.size_bytes()));
      owed -= frames;
    }
  }

  /// The last SDL error, with the audio subsystem shut again.
  std::string failed() {
    std::string reason = SDL_GetError();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return reason.empty() ? "the audio device would not open" : reason;
  }

  /// The default output, opened on @p engine's format and pulling from it,
  /// and started; null if it would not open.
  SDL_AudioStream* openStream(AudioEngine& engine) {
    const SDL_AudioSpec spec{.format = SDL_AUDIO_F32,
                             .channels = 2,
                             .freq = static_cast<int>(engine.sampleRate())};
    SDL_AudioStream* stream =
        SDL_OpenAudioDeviceStream(DEFAULT_PLAYBACK, &spec, feed, &engine);
    if (stream != nullptr) {
      // A device stream opens paused.
      SDL_ResumeAudioStreamDevice(stream);
    }
    return stream;
  }

}  // namespace

AudioDevice::~AudioDevice() {
  close();
}

std::optional<std::string> AudioDevice::open(AudioEngine& engine) {
  if (stream_ != nullptr) {
    return std::nullopt;
  }
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    return failed();
  }
  stream_ = openStream(engine);
  if (stream_ == nullptr) {
    return failed();
  }
  return std::nullopt;
}

void AudioDevice::close() {
  if (stream_ == nullptr) {
    return;
  }
  // Closes the device and waits out a callback in flight.
  SDL_DestroyAudioStream(static_cast<SDL_AudioStream*>(stream_));
  stream_ = nullptr;
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace eng::audio
