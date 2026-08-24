// Stub implementations for all PS5 public API functions.
// These return successful default values so the engine compiles and tests
// can link without the PS5 SDK.

#ifdef ENGINE_PLATFORM_PS5

#include <engine/platform/ps5/gnm-rhi-backend.h>
#include <engine/platform/ps5/ps5-init.h>
#include <engine/platform/ps5/ps5-input.h>
#include <engine/platform/ps5/ps5-networking.h>
#include <engine/platform/ps5/ps5-save-data.h>
#include <engine/platform/ps5/ps5-system.h>
#include <engine/platform/ps5/ps5-trophies.h>
#include <engine/platform/ps5/tempest-audio-backend.h>
#include <utility>

namespace eng {
namespace {

  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
  // Stub module state: mutable globals required by the stub layer to
  // simulate SDK state without an actual PS5 backend.

  /// Whether PS5 platform is available (default false until init).
  bool g_available = false;

  /// Monotonically incrementing PSN session handle counter.
  uint32_t g_next_session_handle = 1;

  /// Whether a save operation is currently in progress.
  bool g_save_in_progress = false;

  /// Stub local user ID value.
  constexpr int32_t STUB_LOCAL_USER_ID = 1;

  /// Bytes-per-megabyte conversion factor.
  constexpr uint64_t BYTES_PER_MB = 1024ULL * 1024ULL;

  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

// ===========================================================================
// ps5-init
// ===========================================================================

std::optional<Ps5PlatformContext>
initPs5Platform(const Ps5PlatformConfig& config) {
  g_available = true;
  Ps5PlatformContext ctx;
  ctx.event_bus = config.event_bus;
  ctx.local_user_id = Ps5UserId{STUB_LOCAL_USER_ID};
  ctx.suspended = false;
  return ctx;
}

void tickPs5(Ps5PlatformContext& /*ctx*/) {}

void shutdownPs5Platform(Ps5PlatformContext& /*ctx*/) {
  g_available = false;
}

bool ps5Available() {
  return g_available;
}

Ps5UserId ps5LocalUserId(const Ps5PlatformContext& ctx) {
  return ctx.local_user_id;
}

// ===========================================================================
// GnmRhiBackend
// ===========================================================================

struct GnmRhiBackend::Impl {
  /// GPU memory budget in bytes.
  uint64_t budget_bytes = 0;
  /// Active render mode.
  Ps5RenderMode render_mode = Ps5RenderMode::PERFORMANCE;
};

GnmRhiBackend::GnmRhiBackend() = default;
GnmRhiBackend::~GnmRhiBackend() = default;

GnmRhiBackend::GnmRhiBackend(GnmRhiBackend&& other) noexcept
  : impl_(std::move(other.impl_)) {}

GnmRhiBackend& GnmRhiBackend::operator=(GnmRhiBackend&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

std::optional<GnmRhiBackend> GnmRhiBackend::create(const Ps5GnmConfig& config) {
  GnmRhiBackend backend;
  backend.impl_ = std::make_unique<Impl>();
  backend.impl_->budget_bytes = config.gpu_memory_budget_mb * BYTES_PER_MB;
  backend.impl_->render_mode = config.render_mode;
  return backend;
}

uint64_t GnmRhiBackend::gpuMemoryUsedBytes() const {
  return 0;
}

uint64_t GnmRhiBackend::gpuMemoryBudgetBytes() const {
  return impl_->budget_bytes;
}

Ps5RenderMode GnmRhiBackend::renderMode() const {
  return impl_->render_mode;
}

void GnmRhiBackend::setRenderMode(Ps5RenderMode mode) {
  impl_->render_mode = mode;
}

// ===========================================================================
// TempestAudioBackend
// ===========================================================================

struct TempestAudioBackend::Impl {
  /// Preferred audio codec.
  Ps5AudioCodecPreference codec_preference =
      Ps5AudioCodecPreference::ATRAC9_PREFERRED;
};

TempestAudioBackend::TempestAudioBackend() = default;
TempestAudioBackend::~TempestAudioBackend() = default;

TempestAudioBackend::TempestAudioBackend(TempestAudioBackend&& other) noexcept
  : impl_(std::move(other.impl_)) {}

TempestAudioBackend&
TempestAudioBackend::operator=(TempestAudioBackend&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

std::unique_ptr<TempestAudioBackend>
TempestAudioBackend::create(const Ps5AudioConfig& config) {
  auto backend = std::unique_ptr<TempestAudioBackend>(
      new TempestAudioBackend());  // NOLINT(modernize-make-unique)
  backend->impl_ = std::make_unique<Impl>();
  backend->impl_->codec_preference = config.codec_preference;
  return backend;
}

uint32_t TempestAudioBackend::activeSourceCount() const {
  return 0;
}

Ps5AudioCodecPreference TempestAudioBackend::codecPreference() const {
  return impl_->codec_preference;
}

// ===========================================================================
// Ps5InputBackend
// ===========================================================================

struct Ps5InputBackend::Impl {
  /// Current vibration on/off state.
  Ps5VibrationState vibration_state = Ps5VibrationState::ENABLED;
};

Ps5InputBackend::Ps5InputBackend() = default;
Ps5InputBackend::~Ps5InputBackend() = default;

Ps5InputBackend::Ps5InputBackend(Ps5InputBackend&& other) noexcept
  : impl_(std::move(other.impl_)) {}

Ps5InputBackend& Ps5InputBackend::operator=(Ps5InputBackend&& other) noexcept {
  impl_ = std::move(other.impl_);
  return *this;
}

std::unique_ptr<Ps5InputBackend>
Ps5InputBackend::create(const Ps5InputConfig& config) {
  auto backend = std::unique_ptr<Ps5InputBackend>(
      new Ps5InputBackend());  // NOLINT(modernize-make-unique)
  backend->impl_ = std::make_unique<Impl>();
  backend->impl_->vibration_state = config.vibration_state;
  return backend;
}

void Ps5InputBackend::setVibrationState(Ps5VibrationState state) {
  impl_->vibration_state = state;
}

Ps5VibrationState Ps5InputBackend::vibrationState() const {
  return impl_->vibration_state;
}

void Ps5InputBackend::setRumble(float /*left*/, float /*right*/) {}

std::expected<void, Ps5Error>
Ps5InputBackend::setTriggerEffect(Ps5TriggerSide /*side*/, Ps5TriggerMode mode,
                                  const Ps5TriggerEffectParams& /*params*/) {
  auto mode_val = static_cast<uint8_t>(mode);
  auto max_val = static_cast<uint8_t>(Ps5TriggerMode::WEAPON);
  if (mode_val > max_val) {
    return std::unexpected(Ps5Error::INVALID_TRIGGER_MODE);
  }
  return {};
}

void Ps5InputBackend::playHaptic(Ps5HapticEvent /*event*/) {}

Ps5ButtonGlyph Ps5InputBackend::getButtonGlyph(Ps5Button button) const {
  return static_cast<Ps5ButtonGlyph>(static_cast<uint8_t>(button));
}

uint32_t Ps5InputBackend::connectedControllerCount() const {
  return PS5_STUB_DEFAULT_CONTROLLER_COUNT;
}

// ===========================================================================
// ps5-networking
// ===========================================================================

std::optional<Ps5NetworkingContext>
initPs5Networking(const Ps5NetworkingConfig& config) {
  Ps5NetworkingContext ctx;
  ctx.event_bus = config.event_bus;
  ctx.network_status = Ps5NetworkStatus::ONLINE;
  ctx.signed_in_user = Ps5UserId{STUB_LOCAL_USER_ID};
  return ctx;
}

void shutdownPs5Networking(Ps5NetworkingContext& /*ctx*/) {}

void tickPs5Networking(Ps5NetworkingContext& /*ctx*/) {}

std::expected<void, Ps5Error> psnSignIn(Ps5NetworkingContext& ctx) {
  ctx.signed_in_user = Ps5UserId{STUB_LOCAL_USER_ID};
  return {};
}

bool psnAvailable(const Ps5NetworkingContext& ctx) {
  return ctx.signed_in_user != PS5_USER_ID_INVALID;
}

Ps5NetworkStatus psnNetworkStatus(const Ps5NetworkingContext& ctx) {
  return ctx.network_status;
}

std::expected<Ps5SessionHandle, Ps5Error>
createSession(Ps5NetworkingContext& ctx) {
  Ps5SessionHandle handle{g_next_session_handle++};
  ctx.active_session = handle;
  return handle;
}

void destroySession(Ps5NetworkingContext& ctx, Ps5SessionHandle /*session*/) {
  ctx.active_session = PS5_SESSION_INVALID;
}

std::expected<void, Ps5Error> sendInvite(const Ps5NetworkingContext& /*ctx*/,
                                         Ps5UserId /*target*/) {
  return {};
}

void updatePresence(const Ps5NetworkingContext& /*ctx*/,
                    std::string_view /*world_name*/,
                    uint32_t /*player_count*/) {}

bool parentalControlAllowsOnline(const Ps5NetworkingContext& /*ctx*/) {
  return true;
}

// ===========================================================================
// ps5-save-data
// ===========================================================================

std::expected<void, Ps5Error> initPs5SaveData(const Ps5SaveConfig& /*config*/) {
  g_save_in_progress = false;
  return {};
}

void shutdownPs5SaveData() {
  g_save_in_progress = false;
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) -- PS5
// stub API; mutates module stub state (g_save_in_progress)
std::expected<void, Ps5Error>
saveWorldAsync(std::string_view /*world_seed*/,
               std::span<const std::byte> /*data*/) {
  g_save_in_progress = true;
  return {};
}

std::expected<void, Ps5Error> loadWorldAsync(std::string_view /*world_seed*/) {
  return {};
}

bool isSaveInProgress() {
  return g_save_in_progress;
}

// ===========================================================================
// ps5-trophies
// ===========================================================================

std::expected<void, Ps5Error>
initPs5Trophies(const Ps5TrophyConfig& /*config*/) {
  return {};
}

void shutdownPs5Trophies() {}

void unlockTrophy(std::string_view /*trophy_id*/) {}

// ===========================================================================
// ps5-system
// ===========================================================================

std::expected<void, Ps5Error> initPs5System(const Ps5SystemConfig& /*config*/) {
  return {};
}

void shutdownPs5System() {}

void tickPs5System(Ps5PlatformContext& /*ctx*/) {}

void registerActivityCard(Ps5ActivityCardType /*type*/,
                          std::string_view /*world_seed*/) {}

void updateActivityCard(Ps5ActivityCardType /*type*/,
                        std::string_view /*world_seed*/) {}

}  // namespace eng

#endif  // ENGINE_PLATFORM_PS5
