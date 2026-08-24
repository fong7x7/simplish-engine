#pragma once

#ifdef ENGINE_RENDERER_METAL

// Internal header — handle-to-Metal-object tracking.
// Not included by any public header.

#include <cstdint>
#include <unordered_map>

namespace eng {

/// Maps opaque uint64_t handles to Metal objects (or any type T).
/// ARC retains Objective-C objects stored in the map and releases
/// them when erased or when the map is destroyed.
/// Main thread only.
template <typename T> class MetalResourceMap {
public:
  /// Store a Metal object under the given handle.
  void insert(uint64_t handle, T obj) { map_[handle] = obj; }

  /// Look up a Metal object by handle. Returns nil/default if not found.
  T lookup(uint64_t handle) const {
    auto it = map_.find(handle);
    if (it == map_.end()) {
      return T{};
    }
    return it->second;
  }

  /// Remove a handle. ARC releases the Metal object.
  void erase(uint64_t handle) { map_.erase(handle); }

  /// Check whether a handle exists.
  bool contains(uint64_t handle) const { return map_.contains(handle); }

private:
  std::unordered_map<uint64_t, T> map_;
};

}  // namespace eng

#endif  // ENGINE_RENDERER_METAL
