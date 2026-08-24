// Stub implementations for all Steam public API functions.
// These return successful default values so the engine compiles and tests
// can link without the Steamworks SDK.

#include <array>
#include <engine/platform/steam/steam-achievements.h>
#include <engine/platform/steam/steam-cloud.h>
#include <engine/platform/steam/steam-init.h>
#include <engine/platform/steam/steam-input.h>
#include <engine/platform/steam/steam-matchmaking.h>
#include <engine/platform/steam/steam-networking.h>
#include <engine/platform/steam/steam-overlay.h>
#include <engine/platform/steam/steam-workshop.h>
#include <map>
#include <string>

namespace eng {
namespace {

  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,bugprone-throwing-static-initialization)
  // Stub module state: mutable globals required by the stub layer to
  // simulate SDK state without an actual Steamworks backend.

  /// Whether Steam platform is available (default true for stubs).
  bool g_available = true;

  /// Display name of the local user.
  std::string g_user_name = "SteamTestUser";

  /// Language code for the local user.
  std::string g_language = "english";

  /// Monotonically incrementing connection handle counter.
  uint32_t g_next_conn_handle = 1;

  /// Monotonically incrementing listener handle counter.
  uint32_t g_next_listener_handle = 1;

  /// Stub local user ID value.
  constexpr uint64_t STUB_LOCAL_USER_ID = 12345;

  /// Stub lobby ID value.
  constexpr uint64_t STUB_LOBBY_ID = 1;

  /// Stub workshop item ID value.
  constexpr uint64_t STUB_WORKSHOP_ITEM_ID = 1000001;

  /// Default stub glyph path for input action queries.
  constexpr std::string_view STUB_GLYPH_PATH = "/steam/glyphs/default.png";

  /// Default stub workshop install path.
  constexpr std::string_view STUB_WORKSHOP_INSTALL_PATH =
      "/steam/workshop/content";

  /// Default stub workshop item size on disk in bytes.
  constexpr uint64_t STUB_WORKSHOP_SIZE_ON_DISK = 1024;

  /// Stub byte value for default cloud file content.
  constexpr std::byte STUB_CLOUD_DEFAULT_BYTE{0x00};

  /// Stub byte value returned for received network messages.
  constexpr std::byte STUB_MESSAGE_BYTE{0x01};

  /// Stub glyph path returned for action queries.
  std::string g_glyph_path = std::string(STUB_GLYPH_PATH);

  /// In-memory cloud storage keyed by filename.
  std::map<std::string, std::vector<std::byte>> g_cloud_files = {
      {"test.cfg", {STUB_CLOUD_DEFAULT_BYTE}},
  };

  /// Stub controller handles cache.
  std::array<SteamInputHandle, 1> g_controller_handles = {SteamInputHandle{1}};

  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,cert-err58-cpp,bugprone-throwing-static-initialization)

}  // namespace

// ---------------------------------------------------------------------------
// steam-init.h
// ---------------------------------------------------------------------------

std::optional<SteamContext> initSteam(const SteamConfig& config) {
  SteamContext ctx;
  ctx.app_id = config.app_id;
  ctx.local_user_id = SteamUserId{STUB_LOCAL_USER_ID};
  ctx.event_bus = config.event_bus;
  g_available = true;
  g_user_name = "SteamTestUser";
  g_language = "english";
  g_glyph_path = std::string(STUB_GLYPH_PATH);
  return ctx;
}

void tickSteamCallbacks(const SteamContext& /*ctx*/) {}

void shutdownSteam(SteamContext& /*ctx*/) {}

bool steamAvailable() {
  return g_available;
}

SteamUserId steamLocalUserId(const SteamContext& ctx) {
  return ctx.local_user_id;
}

std::string_view steamLocalUserName(const SteamContext& /*ctx*/) {
  return g_user_name;
}

std::string_view steamLanguage(const SteamContext& /*ctx*/) {
  return g_language;
}

// ---------------------------------------------------------------------------
// steam-achievements.h
// ---------------------------------------------------------------------------

void unlockAchievement(const SteamContext& /*ctx*/,
                       std::string_view /*achievement_id*/) {}

void setStatInt(const SteamContext& /*ctx*/, std::string_view /*stat_name*/,
                int32_t /*value*/) {}

void setStatFloat(const SteamContext& /*ctx*/, std::string_view /*stat_name*/,
                  float /*value*/) {}

void flushStats(const SteamContext& /*ctx*/) {}

void tickStats(const SteamContext& /*ctx*/, float /*dt*/) {}

// ---------------------------------------------------------------------------
// steam-cloud.h
// ---------------------------------------------------------------------------

std::expected<bool, SteamError> cloudWrite(const SteamContext& /*ctx*/,
                                           std::string_view filename,
                                           std::span<const std::byte> data) {
  if (filename.empty()) {
    return std::unexpected(SteamError::INVALID_ARGUMENT);
  }
  g_cloud_files[std::string(filename)] = {data.begin(), data.end()};
  return true;
}

std::expected<std::vector<std::byte>, SteamError>
cloudRead(const SteamContext& /*ctx*/, std::string_view filename) {
  auto iter = g_cloud_files.find(std::string(filename));
  if (iter == g_cloud_files.end()) {
    return std::unexpected(SteamError::CLOUD_FILE_NOT_FOUND);
  }
  return iter->second;
}

std::expected<bool, SteamError> cloudDelete(const SteamContext& /*ctx*/,
                                            std::string_view filename) {
  g_cloud_files.erase(std::string(filename));
  return true;
}

bool cloudIsEnabled(const SteamContext& /*ctx*/) {
  return true;
}

// ---------------------------------------------------------------------------
// steam-input.h
// ---------------------------------------------------------------------------

std::expected<bool, SteamError>
activateActionSet(const SteamContext& /*ctx*/, SteamInputHandle /*controller*/,
                  std::string_view /*action_set_name*/) {
  return true;
}

std::string_view getGlyphForAction(const SteamContext& /*ctx*/,
                                   SteamInputHandle /*controller*/,
                                   std::string_view /*action_name*/) {
  return g_glyph_path;
}

void setRumble(const SteamContext& /*ctx*/, SteamInputHandle /*controller*/,
               uint16_t /*left_motor*/, uint16_t /*right_motor*/) {}

void setDualSenseTriggerEffect(const SteamContext& /*ctx*/,
                               SteamInputHandle /*controller*/,
                               SteamTriggerEffectMode /*mode*/,
                               const SteamTriggerEffectParams& /*params*/) {}

void showFloatingKeyboard(const SteamContext& /*ctx*/,
                          SteamFloatingKeyboardMode /*mode*/) {}

std::span<const SteamInputHandle>
getControllerHandles(const SteamContext& /*ctx*/) {
  return g_controller_handles;
}

// ---------------------------------------------------------------------------
// steam-matchmaking.h
// ---------------------------------------------------------------------------

std::expected<SteamLobbyId, SteamError> createLobby(const SteamContext& /*ctx*/,
                                                    SteamLobbyType /*type*/,
                                                    uint32_t max_members) {
  if (max_members > STEAM_MAX_LOBBY_MEMBERS) {
    return std::unexpected(SteamError::LOBBY_FULL);
  }
  return SteamLobbyId{STUB_LOBBY_ID};
}

std::expected<bool, SteamError> joinLobby(const SteamContext& /*ctx*/,
                                          SteamLobbyId /*lobby_id*/) {
  return true;
}

void leaveLobby(const SteamContext& /*ctx*/, SteamLobbyId /*lobby_id*/) {}

std::expected<bool, SteamError> inviteToLobby(const SteamContext& /*ctx*/,
                                              SteamLobbyId /*lobby_id*/,
                                              SteamUserId /*friend_id*/) {
  return true;
}

std::expected<bool, SteamError> setLobbyData(const SteamContext& /*ctx*/,
                                             SteamLobbyId /*lobby_id*/,
                                             std::string_view /*key*/,
                                             std::string_view /*value*/) {
  return true;
}

std::expected<bool, SteamError>
setLobbyGameServer(const SteamContext& /*ctx*/, SteamLobbyId /*lobby_id*/,
                   const SteamLobbyGameServerParams& /*server*/) {
  return true;
}

// ---------------------------------------------------------------------------
// steam-networking.h
// ---------------------------------------------------------------------------

std::expected<SteamConnectionHandle, SteamError>
connectRelay(const SteamContext& /*ctx*/, SteamUserId /*remote_user*/,
             uint16_t /*virtual_port*/) {
  return SteamConnectionHandle{g_next_conn_handle++};
}

std::expected<SteamConnectionHandle, SteamError>
connectDirect(const SteamContext& /*ctx*/, std::string_view /*ip*/,
              uint16_t /*port*/) {
  return SteamConnectionHandle{g_next_conn_handle++};
}

std::expected<SteamListenerHandle, SteamError>
listenRelay(const SteamContext& /*ctx*/, uint16_t /*virtual_port*/) {
  return SteamListenerHandle{g_next_listener_handle++};
}

std::expected<SteamListenerHandle, SteamError>
listenDirect(const SteamContext& /*ctx*/, uint16_t /*port*/) {
  return SteamListenerHandle{g_next_listener_handle++};
}

std::expected<bool, SteamError> sendMessage(const SteamContext& /*ctx*/,
                                            SteamConnectionHandle /*conn*/,
                                            std::span<const std::byte> /*data*/,
                                            SteamSendFlags /*flags*/) {
  return true;
}

std::vector<std::vector<std::byte>>
receiveMessages(const SteamContext& /*ctx*/, SteamConnectionHandle /*conn*/,
                uint32_t /*max_messages*/) {
  return {std::vector<std::byte>{STUB_MESSAGE_BYTE}};
}

void closeConnection(const SteamContext& /*ctx*/,
                     SteamConnectionHandle /*conn*/) {}

void closeListener(const SteamContext& /*ctx*/,
                   SteamListenerHandle /*listener*/) {}

SteamConnectionStatus queryConnectionStatus(const SteamContext& /*ctx*/,
                                            SteamConnectionHandle /*conn*/) {
  return SteamConnectionStatus::CONNECTED;
}

// ---------------------------------------------------------------------------
// steam-overlay.h
// ---------------------------------------------------------------------------

void setOverlayNotificationPosition(const SteamContext& /*ctx*/,
                                    SteamNotificationPosition /*position*/) {}

void hookScreenshots(const SteamContext& /*ctx*/) {}

// ---------------------------------------------------------------------------
// steam-workshop.h
// ---------------------------------------------------------------------------

std::expected<bool, SteamError> subscribeItem(const SteamContext& /*ctx*/,
                                              uint64_t /*item_id*/) {
  return true;
}

std::expected<SteamWorkshopItemInfo, SteamError>
getItemInstallInfo(const SteamContext& /*ctx*/, uint64_t item_id) {
  SteamWorkshopItemInfo info;
  info.item_id = item_id;
  info.install_path = std::string(STUB_WORKSHOP_INSTALL_PATH);
  info.size_on_disk = STUB_WORKSHOP_SIZE_ON_DISK;
  info.is_installed = true;
  return info;
}

std::expected<uint64_t, SteamError> createItem(const SteamContext& /*ctx*/) {
  return STUB_WORKSHOP_ITEM_ID;
}

std::expected<bool, SteamError>
submitItemUpdate(const SteamContext& /*ctx*/, uint64_t /*item_id*/,
                 const SteamWorkshopUpdateParams& /*params*/) {
  return true;
}

std::vector<SteamWorkshopItemInfo>
getSubscribedItems(const SteamContext& /*ctx*/) {
  SteamWorkshopItemInfo info;
  info.item_id = STUB_WORKSHOP_ITEM_ID;
  info.install_path = std::string(STUB_WORKSHOP_INSTALL_PATH);
  info.size_on_disk = STUB_WORKSHOP_SIZE_ON_DISK;
  info.is_installed = true;
  return {info};
}

}  // namespace eng
