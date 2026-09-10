#include <engine/render/backends/opengl/opengl-command-list.h>

#ifdef ENGINE_RENDERER_OPENGL

#include <algorithm>
#include <cstring>

namespace eng::render {

void OpenGlCommandList::begin() {
  commands_.clear();
}

void OpenGlCommandList::end() {}

void OpenGlCommandList::beginRenderPass(const RhiRenderPassBeginInfo& info) {
  commands_.emplace_back(GlCmdBeginRenderPass{info});
}

void OpenGlCommandList::endRenderPass() {
  commands_.emplace_back(GlCmdEndRenderPass{});
}

void OpenGlCommandList::bindPipeline(RhiPipelineHandle pipeline) {
  commands_.emplace_back(GlCmdBindPipeline{pipeline});
}

void OpenGlCommandList::bindVertexBuffer(RhiBufferHandle buffer,
                                         uint64_t offset) {
  commands_.emplace_back(GlCmdBindVertexBuffer{buffer, offset});
}

void OpenGlCommandList::bindIndexBuffer(RhiBufferHandle buffer, uint64_t offset,
                                        RhiIndexType index_type) {
  commands_.emplace_back(GlCmdBindIndexBuffer{buffer, offset, index_type});
}

void OpenGlCommandList::setVertexStageBytes(const void* data, size_t size,
                                            uint32_t slot) {
  if (data == nullptr || size == 0) {
    return;
  }
  if (size > sizeof(GlCmdSetVertexStageBytes::data)) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    commands_.emplace_back(GlCmdSetVertexStageBlock{
        slot, std::vector<uint8_t>(bytes, bytes + size)});
    return;
  }
  GlCmdSetVertexStageBytes cmd{};
  cmd.slot = slot;
  cmd.size = static_cast<uint32_t>(size);
  std::memcpy(cmd.data, data, cmd.size);
  commands_.emplace_back(cmd);
}

void OpenGlCommandList::setFragmentStageBytes(const void* data, size_t size,
                                              uint32_t slot) {
  if (data == nullptr || size == 0) {
    return;
  }
  GlCmdSetFragmentStageBytes cmd{};
  cmd.slot = slot;
  cmd.size = static_cast<uint32_t>(
      std::min(size, sizeof(GlCmdSetFragmentStageBytes::data)));
  std::memcpy(cmd.data, data, cmd.size);
  commands_.emplace_back(cmd);
}

void OpenGlCommandList::bindFragmentTexture(RhiTextureHandle texture,
                                            uint32_t slot) {
  commands_.emplace_back(GlCmdBindFragmentTexture{texture, slot});
}

void OpenGlCommandList::bindDescriptorSet(uint32_t set_index,
                                          RhiDescriptorSetHandle set) {
  commands_.emplace_back(GlCmdBindDescriptorSet{set_index, set});
}

void OpenGlCommandList::setViewport(const RhiViewport& viewport) {
  commands_.emplace_back(GlCmdSetViewport{viewport});
}

void OpenGlCommandList::setScissor(const RhiScissor& scissor) {
  commands_.emplace_back(GlCmdSetScissor{scissor});
}

void OpenGlCommandList::draw(const RhiDrawParams& params) {
  commands_.emplace_back(GlCmdDraw{params});
}

void OpenGlCommandList::drawIndexed(const RhiDrawIndexedParams& params) {
  commands_.emplace_back(GlCmdDrawIndexed{params});
}

void OpenGlCommandList::dispatch(uint32_t groups_x, uint32_t groups_y,
                                 uint32_t groups_z) {
  commands_.emplace_back(GlCmdDispatch{groups_x, groups_y, groups_z});
}

void OpenGlCommandList::copyBuffer(const RhiCopyBufferParams& params) {
  commands_.emplace_back(GlCmdCopyBuffer{params});
}

void OpenGlCommandList::copyTextureToBuffer(RhiTextureHandle src,
                                            RhiBufferHandle dst) {
  commands_.emplace_back(GlCmdCopyTextureToBuffer{src, dst});
}

void OpenGlCommandList::textureBarrier(RhiTextureHandle texture,
                                       RhiTextureLayout old_layout,
                                       RhiTextureLayout new_layout) {
  commands_.emplace_back(GlCmdTextureBarrier{texture, old_layout, new_layout});
}

const std::vector<GlCommand>& OpenGlCommandList::commands() const {
  return commands_;
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_OPENGL
