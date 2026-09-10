#include "mesh-gpu-upload.h"

#include <cstring>

namespace eng {

namespace {

  /// Copy CPU memory into an already-created buffer.
  bool fillBuffer(RhiDevice& device, RhiBufferHandle handle, const void* data,
                  uint64_t bytes) {
    void* mapped = device.mapBuffer(handle);
    if (mapped == nullptr) {
      return false;
    }
    std::memcpy(mapped, data, bytes);
    device.unmapBuffer(handle);
    return true;
  }

}  // namespace

RhiBufferHandle uploadMeshBuffer(RhiDevice& device, const void* data,
                                 uint64_t bytes, RhiBufferUsage usage) {
  RhiBufferDesc desc{};
  desc.size = bytes;
  desc.usage = usage;
  desc.host_visible = true;
  desc.debug_name = "mesh_buffer";
  const RhiBufferHandle handle = device.createBuffer(desc);
  if (handle == 0) {
    return 0;
  }
  if (!fillBuffer(device, handle, data, bytes)) {
    device.destroyBuffer(handle);
    return 0;
  }
  return handle;
}

}  // namespace eng
