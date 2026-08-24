#include <chrono>
#include <engine/core/audit/audit-merge-context.h>
#include <memory>
#include <thread>
#include <vector>

namespace eng {

std::optional<AuditMergeContext>
AuditMergeContext::create(uint32_t merge_buffer_capacity) {
  if (merge_buffer_capacity == 0) {
    return std::nullopt;
  }
  AuditMergeContext ctx;
  ctx.merge_buffer = std::make_unique<std::byte[]>(merge_buffer_capacity);
  ctx.merge_buffer_capacity = merge_buffer_capacity;
  return std::move(ctx);
}

void AuditMergeContext::registerThreadBuffer(AuditMergeContext& ctx,
                                             AuditRingBuffer* buffer) {
  const std::scoped_lock lock(ctx.buffer_list_mutex);
  ctx.thread_buffers.push_back(buffer);
}

void AuditMergeContext::unregisterThreadBuffer(AuditMergeContext& ctx,
                                               AuditRingBuffer* buffer) {
  const std::scoped_lock lock(ctx.buffer_list_mutex);
  auto& bufs = ctx.thread_buffers;
  std::erase(bufs, buffer);
}

/// Poll interval for the merge thread's spin loop.
constexpr auto MERGE_POLL_INTERVAL = std::chrono::milliseconds(1);

void AuditMergeContext::startMergeThread(AuditMergeContext& ctx) {
  ctx.running.store(true);
  ctx.merge_thread = std::thread([&ctx]() {
    while (ctx.running.load()) {
      std::this_thread::sleep_for(MERGE_POLL_INTERVAL);
    }
  });
}

void AuditMergeContext::stopMergeThread(AuditMergeContext& ctx) {
  ctx.running.store(false);
  if (ctx.merge_thread.joinable()) {
    ctx.merge_thread.join();
  }
}

void AuditMergeContext::destroy(AuditMergeContext& ctx) {
  AuditMergeContext::stopMergeThread(ctx);
  ctx.merge_buffer.reset();
  ctx.merge_buffer_capacity = 0;
  ctx.thread_buffers.clear();
}

AuditMergeContext&
AuditMergeContext::operator=(AuditMergeContext&& other) noexcept {
  thread_buffers = std::move(other.thread_buffers);
  merge_buffer = std::move(other.merge_buffer);
  merge_buffer_capacity = other.merge_buffer_capacity;
  merge_write_cursor.store(other.merge_write_cursor.load());
  running.store(other.running.load());
  merge_thread = std::move(other.merge_thread);
  other.merge_buffer_capacity = 0;
  return *this;
}

}  // namespace eng
