/// @file vulkan-memory-allocator.cpp
/// @brief The one translation unit that holds VulkanMemoryAllocator's body.
/// @par Threading Not applicable; this file only instantiates the library.

#ifdef ENGINE_RENDERER_VULKAN

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#endif  // ENGINE_RENDERER_VULKAN
