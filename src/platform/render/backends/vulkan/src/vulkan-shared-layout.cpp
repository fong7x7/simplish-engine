#include "vulkan-shared-layout.h"

#ifdef ENGINE_RENDERER_VULKAN

#include <array>

namespace eng::render {

namespace {

  VkSampler createLinearSampler(VkDevice device, VkSamplerAddressMode address) {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = address;
    info.addressModeV = address;
    info.addressModeW = address;
    info.maxLod = VK_LOD_CLAMP_NONE;
    VkSampler sampler = VK_NULL_HANDLE;
    vkCreateSampler(device, &info, nullptr, &sampler);
    return sampler;
  }

  VkDescriptorSetLayoutBinding makeBinding(uint32_t binding,
                                           VkDescriptorType type,
                                           VkShaderStageFlags stages) {
    VkDescriptorSetLayoutBinding b{};
    b.binding = binding;
    b.descriptorType = type;
    b.descriptorCount = 1;
    b.stageFlags = stages;
    return b;
  }

  VkDescriptorSetLayoutBinding makeSamplerBinding(uint32_t binding,
                                                  const VkSampler* sampler) {
    VkDescriptorSetLayoutBinding b = makeBinding(
        binding, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
    b.pImmutableSamplers = sampler;
    return b;
  }

  using SharedBindings =
      std::array<VkDescriptorSetLayoutBinding, VULKAN_SHARED_BINDING_COUNT>;

  void fillUniformBindings(SharedBindings& b) {
    constexpr auto UBO = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    constexpr auto VS = VK_SHADER_STAGE_VERTEX_BIT;
    constexpr auto FS = VK_SHADER_STAGE_FRAGMENT_BIT;
    b[0] = makeBinding(VULKAN_BINDING_VERTEX_UBO0, UBO, VS);
    b[1] = makeBinding(VULKAN_BINDING_VERTEX_UBO1, UBO, VS);
    b[2] = makeBinding(VULKAN_BINDING_VERTEX_UBO2, UBO, VS);
    b[3] = makeBinding(VULKAN_BINDING_FRAGMENT_UBO0, UBO, FS);
    b[4] = makeBinding(VULKAN_BINDING_FRAGMENT_UBO1, UBO, FS);
  }

  VkDescriptorSetLayout createSetLayout(VkDevice device,
                                        const VulkanSharedLayout& l) {
    SharedBindings b{};
    fillUniformBindings(b);
    b[5] = makeBinding(VULKAN_BINDING_FRAGMENT_TEXTURE,
                       VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                       VK_SHADER_STAGE_FRAGMENT_BIT);
    b[6] = makeSamplerBinding(VULKAN_BINDING_CLAMP_SAMPLER, &l.clamp_sampler);
    b[7] = makeSamplerBinding(VULKAN_BINDING_REPEAT_SAMPLER, &l.repeat_sampler);
    VkDescriptorSetLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    info.bindingCount = static_cast<uint32_t>(b.size());
    info.pBindings = b.data();
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    vkCreateDescriptorSetLayout(device, &info, nullptr, &layout);
    return layout;
  }

  /// A pipeline layout over `set_layout`, or over nothing when it is null.
  VkPipelineLayout createPipelineLayout(VkDevice device,
                                        const VkDescriptorSetLayout* set) {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = set != nullptr ? 1 : 0;
    info.pSetLayouts = set;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device, &info, nullptr, &layout);
    return layout;
  }

}  // namespace

bool createVulkanSharedLayout(VkDevice device, VulkanSharedLayout& out) {
  out.clamp_sampler =
      createLinearSampler(device, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
  out.repeat_sampler =
      createLinearSampler(device, VK_SAMPLER_ADDRESS_MODE_REPEAT);
  if (out.clamp_sampler == VK_NULL_HANDLE ||
      out.repeat_sampler == VK_NULL_HANDLE) {
    return false;
  }
  out.set_layout = createSetLayout(device, out);
  if (out.set_layout == VK_NULL_HANDLE) {
    return false;
  }
  out.graphics = createPipelineLayout(device, &out.set_layout);
  out.compute = createPipelineLayout(device, nullptr);
  return out.graphics != VK_NULL_HANDLE && out.compute != VK_NULL_HANDLE;
}

void destroyVulkanSharedLayout(VkDevice device, VulkanSharedLayout& layout) {
  vkDestroyPipelineLayout(device, layout.compute, nullptr);
  vkDestroyPipelineLayout(device, layout.graphics, nullptr);
  vkDestroyDescriptorSetLayout(device, layout.set_layout, nullptr);
  vkDestroySampler(device, layout.repeat_sampler, nullptr);
  vkDestroySampler(device, layout.clamp_sampler, nullptr);
  layout = VulkanSharedLayout{};
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_VULKAN
