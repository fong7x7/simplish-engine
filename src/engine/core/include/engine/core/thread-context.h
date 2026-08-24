#pragma once

#include <cstdint>
#include <engine/core/event-bus.h>
#include <string_view>
#include <thread>

namespace eng {

// Forward declarations required: these types live in higher dependency layers
// (world-storage Layer 1) that core (Layer 0) cannot depend on.
// ThreadPool has no header definition yet.
class ChunkManager;  // NOLINT(no-forward-decl) avoids circular dependency
class ThreadPool;    // NOLINT(no-forward-decl) avoids circular dependency

// ============================================================================
// DESIGN SUMMARY
// ============================================================================
// ThreadContext: Thread-local state and role information.
//
// Responsibilities:
// - Provide thread-local access to engine subsystems (chunk manager, event bus,
// pool)
// - Identify thread role (main, render, io, worker, async-plugin)
// - Enforce thread-safety contracts at runtime
//
// Key Invariants:
// - Each thread has at most one ThreadContext (set once at thread start)
// - ThreadContext is thread-local; safe to read without locks
// - Main thread context is always available; other threads may not have context
// - Worker/async threads can query their pool without storing it
//
// Usage (in worker/async thread):
//   auto& ctx = ThreadContext::get();  // Get current thread's context
//   ctx.chunk_manager->load(...);      // Safe; only called by I/O thread
//   ctx.event_bus->emit(...);          // ERROR; emit only on main thread
// ============================================================================

class ThreadContext {
public:
  enum class Role {
    MAIN,          // Main simulation thread
    RENDER,        // GPU command submission thread
    IO,            // Chunk I/O thread
    WORKER,        // Worker pool task execution
    ASYNC_PLUGIN,  // Async plugin execution thread
    OTHER,         // Unregistered thread
  };

  // Get thread-local context for current thread
  // Returns nullptr if thread has no registered context
  static ThreadContext* get();

  // Create and register context for current thread
  // Called once per thread at startup
  static ThreadContext* create(std::string_view thread_name, Role role);

  // Destroy context for current thread
  // Called at thread shutdown
  static void destroy();

  // Read-only accessors
  std::string_view name() const { return name_; }
  Role role() const { return role_; }
  std::thread::id id() const { return id_; }

  // Subsystem access (may be nullptr depending on role)
  ChunkManager* chunkManager() { return chunk_manager_; }
  EventBus* eventBus() { return event_bus_; }
  ThreadPool* pool() { return pool_; }

  // Assertions for contract enforcement
  // Asserts if called on wrong thread, logs at Error on release
  void assertMainThread(std::string_view operation) const;
  void assertRenderThread(std::string_view operation) const;
  void assertIoThread(std::string_view operation) const;
  void assertWorkerThread(std::string_view operation) const;
  void assertNotMainThread(std::string_view operation) const;

private:
  /// Private tag type to restrict constructor access while allowing
  /// make_unique.
  struct PrivateTag {};

public:
  /// Effectively private constructor — only code with access to PrivateTag
  /// (i.e. ThreadContext members and friends) can call this.
  ThreadContext(PrivateTag tag, std::string_view name, Role role);

private:
  /// Human-readable name for this thread (e.g. "worker-3").
  std::string name_;
  /// Role classification for this thread.
  Role role_;
  /// Platform thread ID captured at creation time.
  std::thread::id id_;
  /// Chunk manager pointer for I/O threads (nullptr for other roles).
  ChunkManager* chunk_manager_ = nullptr;
  /// Event bus pointer for threads that may emit events.
  EventBus* event_bus_ = nullptr;
  /// Thread pool pointer for worker threads.
  ThreadPool* pool_ = nullptr;

  friend class Engine;  // Allow Engine to set subsystem pointers
};

}  // namespace eng
