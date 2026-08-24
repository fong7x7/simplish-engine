// Stub implementations for all Epic public API functions.
// These return successful default values so the engine compiles and tests
// can link without the EOS SDK.

#ifdef ENGINE_PLATFORM_DESKTOP

#include <engine/core/expected-polyfill.h>
#include <engine/platform/epic/epic-achievements.h>
#include <engine/platform/epic/epic-cloud.h>
#include <engine/platform/epic/epic-commerce.h>
#include <engine/platform/epic/epic-init.h>
#include <engine/platform/epic/epic-matchmaking.h>
#include <engine/platform/epic/epic-networking.h>
#include <engine/platform/epic/epic-overlay.h>
#include <engine/platform/epic/epic-social.h>

namespace eng {
namespace {

  // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
  // Stub module state: mutable globals required by the stub layer to
  // simulate SDK state without an actual EOS backend.

  /// Whether Epic platform is available (default true for stubs).
  bool g_available = true;

  /// Display name of the local user.
  std::string g_user_name;

  /// Language code for the local user.
  std::string g_language;

  /// Monotonically incrementing P2P connection handle counter.
  uint32_t g_next_conn_handle = 1;

  /// Stub friend count.
  constexpr uint32_t STUB_FRIEND_COUNT = 3;

  /// Stub local user ID value.
  constexpr uint64_t STUB_LOCAL_USER_ID = 12345;

  /// Stub friend base user ID.
  constexpr uint64_t STUB_FRIEND_BASE_ID = 90000;

  /// Stub lobby ID value.
  constexpr uint64_t STUB_LOBBY_ID = 1;

  /// Stub entitlement count.
  constexpr uint32_t STUB_ENTITLEMENT_COUNT = 1;

  /// Default stub display name for the local user.
  constexpr std::string_view STUB_USER_NAME = "EpicTestUser";

  /// Default stub language code.
  constexpr std::string_view STUB_LANGUAGE = "en";

  /// Stub byte value returned for cloud read operations.
  constexpr std::byte STUB_CLOUD_READ_BYTE{0x42};

  /// Stub byte value returned for received P2P packets.
  constexpr std::byte STUB_PACKET_BYTE{0x01};

  // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

// ---------------------------------------------------------------------------
// epic-init.h
// ---------------------------------------------------------------------------

std::optional<EpicContext> initEpic(const EpicConfig& config) {
  EpicContext ctx;
  ctx.local_user_id = EpicProductUserId{STUB_LOCAL_USER_ID};
  ctx.event_bus = config.event_bus;
  g_available = true;
  g_user_name = std::string(STUB_USER_NAME);
  g_language = std::string(STUB_LANGUAGE);
  return ctx;
}

void tickEpicCallbacks(const EpicContext& /*ctx*/) {}

void shutdownEpic(EpicContext& /*ctx*/) {}

bool epicAvailable() {
  return g_available;
}

EpicProductUserId epicLocalUserId(const EpicContext& ctx) {
  return ctx.local_user_id;
}

std::string_view epicLocalUserName(const EpicContext& /*ctx*/) {
  return g_user_name;
}

std::string_view epicLanguage(const EpicContext& /*ctx*/) {
  return g_language;
}

// ---------------------------------------------------------------------------
// epic-achievements.h
// ---------------------------------------------------------------------------

void epicUnlockAchievement(const EpicContext& /*ctx*/,
                           std::string_view /*achievement_id*/) {}

void epicIngestStat(const EpicContext& /*ctx*/, std::string_view /*stat_name*/,
                    int32_t /*amount*/) {}

void epicFlushStats(const EpicContext& /*ctx*/) {}

void epicQueryLeaderboardRanks(const EpicContext& /*ctx*/,
                               std::string_view /*leaderboard_id*/,
                               uint32_t /*max_results*/,
                               const EpicLeaderboardCallback& callback) {
  callback(std::expected<std::vector<EpicLeaderboardEntry>, EpicError>{
      std::vector<EpicLeaderboardEntry>{}});
}

void epicQueryLeaderboardFriends(const EpicContext& /*ctx*/,
                                 std::string_view /*leaderboard_id*/,
                                 const EpicLeaderboardCallback& callback) {
  callback(std::expected<std::vector<EpicLeaderboardEntry>, EpicError>{
      std::vector<EpicLeaderboardEntry>{}});
}

// ---------------------------------------------------------------------------
// epic-cloud.h
// ---------------------------------------------------------------------------

void epicCloudWrite(const EpicContext& /*ctx*/, std::string_view filename,
                    std::span<const std::byte> /*data*/,
                    const EpicCloudWriteCallback& callback) {
  callback(filename, EpicError::SUCCESS);
}

void epicCloudRead(const EpicContext& /*ctx*/, std::string_view filename,
                   const EpicCloudReadCallback& callback) {
  callback(filename, std::expected<std::vector<std::byte>, EpicError>{
                         std::vector<std::byte>{STUB_CLOUD_READ_BYTE}});
}

void epicCloudDelete(const EpicContext& /*ctx*/, std::string_view filename,
                     const EpicCloudDeleteCallback& callback) {
  callback(filename, EpicError::SUCCESS);
}

void epicCloudQueryFiles(const EpicContext& /*ctx*/,
                         const EpicCloudQueryCallback& callback) {
  callback(std::expected<std::vector<std::string>, EpicError>{
      std::vector<std::string>{}});
}

// ---------------------------------------------------------------------------
// epic-commerce.h
// ---------------------------------------------------------------------------

void epicQueryEntitlements(const EpicContext& /*ctx*/,
                           const EpicEntitlementQueryCallback& callback) {
  callback(std::expected<uint32_t, EpicError>{STUB_ENTITLEMENT_COUNT});
}

bool epicOwnsEntitlement(const EpicContext& /*ctx*/,
                         std::string_view /*entitlement_name*/) {
  return true;
}

void epicRedeemEntitlement(const EpicContext& /*ctx*/,
                           std::string_view /*entitlement_name*/,
                           const EpicRedeemCallback& callback) {
  callback(EpicError::SUCCESS);
}

// ---------------------------------------------------------------------------
// epic-matchmaking.h
// ---------------------------------------------------------------------------

void epicCreateLobby(const EpicContext& /*ctx*/,
                     const EpicLobbyConfig& /*config*/,
                     const EpicLobbyCreateCallback& callback) {
  callback(std::expected<EpicLobbyId, EpicError>{EpicLobbyId{STUB_LOBBY_ID}});
}

void epicJoinLobby(const EpicContext& /*ctx*/, EpicLobbyId lobby_id,
                   const EpicLobbyJoinCallback& callback) {
  callback(std::expected<EpicLobbyId, EpicError>{lobby_id});
}

void epicSendLobbyInvite(const EpicContext& /*ctx*/, EpicLobbyId /*lobby_id*/,
                         EpicProductUserId /*target_user*/) {}

void epicSetLobbyAttribute(const EpicContext& /*ctx*/, EpicLobbyId /*lobby_id*/,
                           std::string_view /*key*/,
                           std::string_view /*value*/) {}

void epicLeaveLobby(const EpicContext& /*ctx*/, EpicLobbyId /*lobby_id*/) {}

void epicSearchSessions(const EpicContext& /*ctx*/,
                        std::string_view /*search_param*/,
                        uint32_t /*max_results*/,
                        const EpicSessionSearchCallback& callback) {
  callback(std::expected<std::vector<EpicSessionInfo>, EpicError>{
      std::vector<EpicSessionInfo>{}});
}

// ---------------------------------------------------------------------------
// epic-networking.h
// ---------------------------------------------------------------------------

std::expected<EpicConnectionHandle, EpicError>
epicConnectP2P(const EpicContext& /*ctx*/, EpicProductUserId /*remote_user*/,
               const EpicP2PConfig& /*config*/) {
  return EpicConnectionHandle{g_next_conn_handle++};
}

std::expected<bool, EpicError>
epicSendPacket(const EpicContext& /*ctx*/, EpicConnectionHandle /*conn*/,
               const EpicPacketParams& /*packet*/) {
  return true;
}

std::vector<std::vector<std::byte>>
epicReceivePackets(const EpicContext& /*ctx*/, EpicConnectionHandle /*conn*/,
                   uint32_t /*max_packets*/) {
  return {std::vector<std::byte>{STUB_PACKET_BYTE}};
}

void epicCloseP2P(const EpicContext& /*ctx*/, EpicConnectionHandle /*conn*/) {}

EpicConnectionStatus epicQueryP2PStatus(const EpicContext& /*ctx*/,
                                        EpicConnectionHandle /*conn*/) {
  return EpicConnectionStatus::CONNECTED;
}

// ---------------------------------------------------------------------------
// epic-overlay.h
// ---------------------------------------------------------------------------

void epicSetNotificationPosition(const EpicContext& /*ctx*/,
                                 EpicNotificationPosition /*position*/) {}

bool epicOverlayPresent(const EpicContext& /*ctx*/) {
  return true;
}

// ---------------------------------------------------------------------------
// epic-social.h
// ---------------------------------------------------------------------------

void epicQueryFriends(const EpicContext& /*ctx*/,
                      const EpicFriendsQueryCallback& callback) {
  callback(std::expected<uint32_t, EpicError>{STUB_FRIEND_COUNT});
}

uint32_t epicGetFriendCount(const EpicContext& /*ctx*/) {
  return STUB_FRIEND_COUNT;
}

EpicProductUserId epicGetFriendAtIndex(const EpicContext& /*ctx*/,
                                       uint32_t index) {
  return EpicProductUserId{STUB_FRIEND_BASE_ID + index};
}

EpicFriendStatus epicGetFriendStatus(const EpicContext& /*ctx*/,
                                     EpicProductUserId /*user*/) {
  return EpicFriendStatus::FRIENDS;
}

void epicSetPresence(const EpicContext& /*ctx*/,
                     const EpicPresenceInfo& /*presence*/) {}

}  // namespace eng

#endif  // ENGINE_PLATFORM_DESKTOP
