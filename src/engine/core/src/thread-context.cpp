#include <engine/core/assert.h>
#include <engine/core/thread-context.h>
#include <memory>

namespace eng {

namespace {

  thread_local std::unique_ptr<ThreadContext>
      t_context;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
                  // per-thread context

}  // namespace

ThreadContext* ThreadContext::get() {
  return t_context.get();
}

ThreadContext* ThreadContext::create(std::string_view thread_name, Role role) {
  t_context = std::make_unique<ThreadContext>(PrivateTag{}, thread_name, role);
  return t_context.get();
}

void ThreadContext::destroy() {
  t_context.reset();
}

ThreadContext::ThreadContext(PrivateTag /*unused*/,
                             std::string_view thread_name, Role role)
  : name_(thread_name), role_(role), id_(std::this_thread::get_id()) {}

void ThreadContext::assertMainThread(std::string_view operation) const {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(role_ == Role::MAIN,
                std::string(operation) + " requires main thread");
}

void ThreadContext::assertRenderThread(std::string_view operation) const {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(role_ == Role::RENDER,
                std::string(operation) + " requires render thread");
}

void ThreadContext::assertIoThread(std::string_view operation) const {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(role_ == Role::IO,
                std::string(operation) + " requires io thread");
}

void ThreadContext::assertWorkerThread(std::string_view operation) const {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(role_ == Role::WORKER,
                std::string(operation) + " requires worker thread");
}

void ThreadContext::assertNotMainThread(std::string_view operation) const {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-do-while) -- macro idiom
  ENGINE_ASSERT(role_ != Role::MAIN,
                std::string(operation) + " must not run on main thread");
}

}  // namespace eng
