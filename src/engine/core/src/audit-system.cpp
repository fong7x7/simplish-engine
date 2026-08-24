#include <engine/core/audit/audit-emit.h>
#include <engine/core/audit/audit-errors.h>
#include <engine/core/audit/audit-merge-context.h>
#include <engine/core/audit/audit-persistence.h>
#include <engine/core/audit/audit-query.h>
#include <engine/core/audit/audit-ring-buffer.h>
#include <engine/core/audit/audit-schema.h>
#include <engine/core/audit/audit-system.h>
#include <span>

namespace eng {

namespace {

  /// Bytes per megabyte for buffer size calculations.
  constexpr uint32_t BYTES_PER_MB = 1024 * 1024;
  /// Default merge buffer capacity: 16 MB.
  constexpr uint32_t DEFAULT_MERGE_CAPACITY = 16 * BYTES_PER_MB;

  /// Thread-local ring buffer for audit emission.
  thread_local std::optional<AuditRingBuffer>
      t_thread_buffer;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
                        // per-thread arena allocator

  /// Compute merge buffer capacity from config.
  uint32_t computeMergeCapacity(uint32_t ring_buffer_size_mb) {
    if (ring_buffer_size_mb == 0) {
      return DEFAULT_MERGE_CAPACITY;
    }
    return ring_buffer_size_mb * BYTES_PER_MB;
  }

  /// Initialize registries and merge context.
  // Algorithm: Load schema/error registries; size merge buffer; create merge.
  bool initSubsystems(AuditSystemContext& sys, std::string_view data_path) {
    auto schema = loadSchemaRegistry(data_path);
    if (!schema) {
      return false;
    }
    auto errors = loadErrorRegistry(data_path);
    if (!errors) {
      return false;
    }
    const auto cap = computeMergeCapacity(sys.config.ring_buffer_size_mb);
    auto merge = AuditMergeContext::create(cap);
    if (!merge) {
      return false;
    }
    sys.schema_registry = std::move(*schema);
    sys.error_registry = std::move(*errors);
    sys.merge_ctx = std::move(*merge);
    return true;
  }

}  // namespace

std::optional<AuditSystemContext> initAuditSystem(const AuditConfig& config,
                                                  TimestampFn timestamp_fn,
                                                  uint64_t* frame_counter,
                                                  std::string_view data_path) {
  AuditSystemContext sys;
  sys.config = config;
  sys.timestamp_fn = timestamp_fn;
  sys.frame_counter = frame_counter;
  if (!initSubsystems(sys, data_path)) {
    return std::nullopt;
  }
  if (config.persist_to_disk) {
    auto pers = createPersistenceContext(config);
    if (pers) {
      sys.persistence_ctx = std::move(*pers);
    }
  }
  sys.query_ctx = createQueryContext(sys.merge_ctx, sys.schema_registry,
                                     sys.error_registry);
  return std::move(sys);
}

void shutdownAuditSystem(AuditSystemContext& ctx) {
  AuditMergeContext::stopMergeThread(ctx.merge_ctx);
  shutdownPersistence(ctx.persistence_ctx);
  AuditMergeContext::destroy(ctx.merge_ctx);
}

void setAuditEnabled(AuditSystemContext& ctx, AuditToggle toggle) {
  ctx.config.enabled = (toggle == AuditToggle::ENABLED);
  const auto emit_toggle =
      ctx.config.enabled ? AuditEmitToggle::ENABLED : AuditEmitToggle::DISABLED;
  setAuditEmitEnabled(emit_toggle);
}

void setCategoryEnabled(AuditSystemContext& ctx, uint16_t category,
                        AuditToggle toggle) {
  if (toggle == AuditToggle::DISABLED) {
    ctx.disabled_categories.insert(category);
  } else {
    ctx.disabled_categories.erase(category);
  }
}

bool registerAuditThread(AuditSystemContext& ctx) {
  constexpr uint32_t THREAD_BUFFER_SIZE = 1 * BYTES_PER_MB;
  auto buf = AuditRingBuffer::create(THREAD_BUFFER_SIZE);
  if (!buf) {
    return false;
  }
  t_thread_buffer = std::move(*buf);
  AuditMergeContext::registerThreadBuffer(ctx.merge_ctx, &*t_thread_buffer);
  return true;
}

void deregisterAuditThread(AuditSystemContext& ctx) {
  if (t_thread_buffer) {
    AuditMergeContext::unregisterThreadBuffer(ctx.merge_ctx, &*t_thread_buffer);
    AuditRingBuffer::destroy(*t_thread_buffer);
    t_thread_buffer.reset();
  }
}

const AuditQueryContext& getAuditQueryContext(const AuditSystemContext& ctx) {
  return ctx.query_ctx;
}

AuditQueryContext& getAuditQueryContextMut(AuditSystemContext& ctx) {
  return ctx.query_ctx;
}

void flushAuditToDisk(AuditSystemContext& ctx) {
  if (ctx.persistence_ctx.current_file == nullptr) {
    return;
  }
  const auto bytes_used = ctx.merge_ctx.merge_write_cursor.load();
  if (bytes_used == 0) {
    return;
  }
  const auto span =
      std::span<const std::byte>(ctx.merge_ctx.merge_buffer.get(), bytes_used);
  flushToDisk(ctx.persistence_ctx, span);
}

}  // namespace eng
