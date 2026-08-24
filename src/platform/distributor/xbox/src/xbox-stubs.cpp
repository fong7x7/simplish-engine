#ifdef ENGINE_PLATFORM_XBOX

#include <engine/platform/xbox/xbox-achievements.h>
#include <engine/platform/xbox/xbox-audio.h>
#include <engine/platform/xbox/xbox-dx12-helpers.h>
#include <engine/platform/xbox/xbox-init.h>
#include <engine/platform/xbox/xbox-input.h>
#include <engine/platform/xbox/xbox-networking.h>
#include <engine/platform/xbox/xbox-save-data.h>
#include <string>
#include <vector>

namespace eng {

namespace {

  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,bugprone-throwing-static-initialization)
  // Stub module state: mutable globals required by the stub layer to
  // simulate SDK state without an actual Xbox GDK backend.

  /// Whether Xbox platform is available (default false until init).
  bool g_xbox_available = false;

  /// Display name of the local user.
  std::string g_local_user_name;

  /// Monotonically incrementing sound handle counter.
  uint32_t g_next_sound_handle = 1;

  /// Monotonically incrementing session handle counter.
  uint32_t g_next_session_handle = 1;

  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,bugprone-throwing-static-initialization)

  constexpr uint64_t STUB_USER_ID_VALUE = 12345;

}  // namespace

// ===========================================================================
// xbox-init.h stubs
// ===========================================================================

std::optional<XboxContext> initXbox(const XboxConfig& config) {
  g_xbox_available = true;
  g_local_user_name = "XboxTestUser";
  return XboxContext{
      .local_user_id = XboxUserId{STUB_USER_ID_VALUE},
      .event_bus = config.event_bus,
      .lifecycle_state = XboxLifecycleState::RUNNING,
  };
}

void tickXbox(const XboxContext& /*ctx*/) {}

void shutdownXbox(XboxContext& /*ctx*/) {}

bool xboxAvailable() {
  return g_xbox_available;
}

XboxUserId xboxLocalUserId(const XboxContext& ctx) {
  return ctx.local_user_id;
}

std::string_view xboxLocalUserName(const XboxContext& /*ctx*/) {
  return g_local_user_name;
}

XboxLifecycleState xboxLifecycleState(const XboxContext& ctx) {
  return ctx.lifecycle_state;
}

// ===========================================================================
// xbox-audio.h stubs
// ===========================================================================

std::expected<XboxAudioContext, XboxError>
initXboxAudio(const XboxAudioConfig& config) {
  return XboxAudioContext{
      .initialised = true,
      .max_spatial_objects = config.max_spatial_objects,
      .active_spatial_objects = 0,
  };
}

void shutdownXboxAudio(XboxAudioContext& ctx) {
  ctx.initialised = false;
}

std::optional<XboxSoundHandle>
playXboxSpatialSound(XboxAudioContext& /*ctx*/,
                     const XboxSoundPlayParams& /*params*/) {
  return XboxSoundHandle{g_next_sound_handle++};
}

void updateXboxSpatialPosition(XboxAudioContext& /*ctx*/,
                               XboxSoundHandle /*handle*/,
                               const float /*position*/[3]) {}

void stopXboxSound(XboxAudioContext& /*ctx*/, XboxSoundHandle /*handle*/) {}

void setXboxReverbPreset(XboxAudioContext& /*ctx*/,
                         XboxReverbPreset /*preset*/) {}

void pauseXboxAudio(XboxAudioContext& /*ctx*/) {}

void resumeXboxAudio(XboxAudioContext& /*ctx*/) {}

void tickXboxAudio(XboxAudioContext& /*ctx*/) {}

// ===========================================================================
// xbox-input.h stubs
// ===========================================================================

std::expected<bool, XboxError>
initXboxInput(const XboxInputConfig& /*config*/) {
  return true;
}

void shutdownXboxInput() {}

XboxControllerState pollXboxInput(uint32_t controller_index) {
  return XboxControllerState{.connected = (controller_index == 0)};
}

void setXboxRumble(uint32_t /*controller_index*/, float /*left*/,
                   float /*right*/) {}

void setXboxTriggerRumble(uint32_t /*controller_index*/,
                          XboxTriggerSide /*side*/, float /*intensity*/) {}

void setXboxVibrationEnabled(XboxVibrationToggle /*toggle*/) {}

uint32_t getXboxConnectedControllerCount() {
  return 1;
}

// ===========================================================================
// xbox-dx12-helpers.h stubs
// ===========================================================================

std::expected<XboxDx12Resources, XboxError>
createXboxDevice(const XboxDx12Config& /*config*/) {
  return XboxDx12Resources{.device_valid = true};
}

std::expected<bool, XboxError>
createXboxSwapChain(XboxDx12Resources& resources,
                    const XboxDx12Config& /*config*/) {
  resources.swap_chain_valid = true;
  return true;
}

std::expected<bool, XboxError>
allocateXboxMemoryPool(XboxDx12Resources& /*resources*/,
                       XboxMemoryPool /*pool*/, uint64_t /*size_bytes*/) {
  return true;
}

std::expected<bool, XboxError>
initXboxDirectStorage(XboxDx12Resources& resources) {
  resources.direct_storage_valid = true;
  return true;
}

void shutdownXboxDx12(XboxDx12Resources& resources) {
  resources.device_valid = false;
  resources.swap_chain_valid = false;
  resources.direct_storage_valid = false;
}

// ===========================================================================
// xbox-networking.h stubs
// ===========================================================================

std::expected<XboxNetworkingContext, XboxError>
initXboxNetworking(const XboxContext& ctx, EventBus* event_bus) {
  return XboxNetworkingContext{
      .local_user_id = ctx.local_user_id,
      .network_state = XboxNetworkState::CONNECTED,
      .event_bus = event_bus,
  };
}

void shutdownXboxNetworking(XboxNetworkingContext& /*ctx*/) {}

std::expected<XboxUserId, XboxError>
signInXboxUser(XboxNetworkingContext& /*ctx*/) {
  return XboxUserId{STUB_USER_ID_VALUE};
}

std::expected<XboxSessionHandle, XboxError>
createXboxSession(XboxNetworkingContext& /*ctx*/,
                  const XboxSessionParams& /*params*/) {
  return XboxSessionHandle{g_next_session_handle++};
}

void destroyXboxSession(XboxNetworkingContext& /*ctx*/,
                        XboxSessionHandle /*session*/) {}

void sendXboxInvite(XboxNetworkingContext& /*ctx*/,
                    XboxSessionHandle /*session*/) {}

void setXboxPresence(XboxNetworkingContext& /*ctx*/,
                     std::string_view /*presence_string*/) {}

XboxPrivilegeResult checkXboxPrivilege(const XboxNetworkingContext& /*ctx*/,
                                       XboxPrivilege /*privilege*/) {
  return XboxPrivilegeResult::ALLOWED;
}

XboxNetworkState xboxNetworkState(const XboxNetworkingContext& ctx) {
  return ctx.network_state;
}

void tickXboxNetworking(XboxNetworkingContext& /*ctx*/) {}

// ===========================================================================
// xbox-save-data.h stubs
// ===========================================================================

std::expected<XboxSaveContext, XboxError>
initXboxSaveProvider(const XboxSaveConfig& config) {
  return XboxSaveContext{
      .provider_valid = true,
      .user_id = config.user_id,
  };
}

void shutdownXboxSaveProvider(XboxSaveContext& ctx) {
  ctx.provider_valid = false;
}

void saveXboxData(XboxSaveContext& /*ctx*/, std::string_view /*container_name*/,
                  std::span<const std::byte> /*data*/,
                  XboxSaveCallback callback) {
  if (callback) {
    callback(true);
  }
}

void loadXboxData(XboxSaveContext& /*ctx*/, std::string_view /*container_name*/,
                  XboxLoadCallback callback) {
  if (callback) {
    callback(std::vector<std::byte>{std::byte{0}});
  }
}

std::expected<bool, XboxError>
deleteXboxSaveContainer(XboxSaveContext& /*ctx*/,
                        std::string_view /*container_name*/) {
  return true;
}

void flushXboxPendingSaves(XboxSaveContext& /*ctx*/) {}

// ===========================================================================
// xbox-achievements.h stubs
// ===========================================================================

void unlockXboxAchievement(const XboxContext& /*ctx*/,
                           std::string_view /*achievement_id*/) {}

void setXboxAchievementProgress(const XboxContext& /*ctx*/,
                                std::string_view /*achievement_id*/,
                                uint32_t /*percent*/) {}

}  // namespace eng

#endif  // ENGINE_PLATFORM_XBOX
