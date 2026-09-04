#ifndef VK_LIBRETRO_H
#define VK_LIBRETRO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "libretro.h"

#if defined(__has_include)
#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#define HAS_SYSTEM_VULKAN 1
#endif
#endif

#ifndef HAS_SYSTEM_VULKAN

#define VK_DEFINE_HANDLE(object) typedef struct object##_T* object;
#if defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__)
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T* object;
#else
#define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef uint64_t object;
#endif

#define VK_NULL_HANDLE 0
#define VK_SUCCESS 0

VK_DEFINE_HANDLE(VkInstance)
VK_DEFINE_HANDLE(VkPhysicalDevice)
VK_DEFINE_HANDLE(VkDevice)
VK_DEFINE_HANDLE(VkQueue)
VK_DEFINE_HANDLE(VkCommandBuffer)

VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSemaphore)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFence)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDeviceMemory)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBuffer)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImage)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkImageView)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkShaderModule)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkPipeline)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkPipelineLayout)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSampler)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorSetLayout)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorPool)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkDescriptorSet)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkRenderPass)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkFramebuffer)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkCommandPool)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkSurfaceKHR)

typedef uint32_t VkFlags;
typedef uint64_t VkDeviceSize;
typedef int32_t VkResult;
typedef uint32_t VkStructureType;
typedef uint32_t VkFormat;
typedef uint32_t VkImageLayout;
typedef uint32_t VkImageUsageFlags;
typedef uint32_t VkQueueFlags;

#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 1
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 2
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 3
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 5
#define VK_STRUCTURE_TYPE_FENCE_CREATE_INFO 8
#define VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 9
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO 14
#define VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO 15
#define VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO 16
#define VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO 18
#define VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO 19
#define VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO 20
#define VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO 22
#define VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO 23
#define VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO 24
#define VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO 26
#define VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO 28
#define VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO 29
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO 30
#define VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO 31
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO 34
#define VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET 35
#define VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO 37
#define VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO 38
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 42
#define VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO 43

#define VK_IMAGE_LAYOUT_UNDEFINED 0
#define VK_IMAGE_LAYOUT_GENERAL 1
#define VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL 2
#define VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 5
#define VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL 6
#define VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL 7
#define VK_IMAGE_LAYOUT_PRESENT_SRC_KHR 1000001002

#define VK_QUEUE_GRAPHICS_BIT 0x00000001
#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_QUEUE_TRANSFER_BIT 0x00000004

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);
typedef PFN_vkVoidFunction (*PFN_vkGetDeviceProcAddr)(VkDevice device, const char* pName);

typedef struct VkApplicationInfo {
	VkStructureType sType;
	const void* pNext;
	const char* pApplicationName;
	uint32_t applicationVersion;
	const char* pEngineName;
	uint32_t engineVersion;
	uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	const VkApplicationInfo* pApplicationInfo;
	uint32_t enabledLayerCount;
	const char* const* ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char* const* ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkPhysicalDeviceFeatures {
	uint32_t robustBufferAccess;
	uint32_t fullDrawIndexUint32;
	uint32_t imageCubeArray;
	uint32_t independentBlend;
	uint32_t geometryShader;
	uint32_t tessellationShader;
	uint32_t sampleRateShading;
	uint32_t dualSrcBlend;
	uint32_t logicOp;
	uint32_t multiDrawIndirect;
	uint32_t drawIndirectFirstInstance;
	uint32_t depthClamp;
	uint32_t depthBiasClamp;
	uint32_t fillModeNonSolid;
	uint32_t depthBounds;
	uint32_t wideLines;
	uint32_t largePoints;
	uint32_t alphaToOne;
	uint32_t multiViewport;
	uint32_t samplerAnisotropy;
	uint32_t textureCompressionETC2;
	uint32_t textureCompressionASTC_LDR;
	uint32_t textureCompressionBC;
	uint32_t occlusionQueryPrecise;
	uint32_t pipelineStatisticsQuery;
	uint32_t vertexPipelineStoresAndAtomics;
	uint32_t fragmentStoresAndAtomics;
	uint32_t shaderTessellationAndGeometryPointSize;
	uint32_t shaderImageGatherExtended;
	uint32_t shaderStorageImageExtendedFormats;
	uint32_t shaderStorageImageMultisample;
	uint32_t shaderStorageImageReadWithoutFormat;
	uint32_t shaderStorageImageWriteWithoutFormat;
	uint32_t shaderUniformBufferArrayDynamicIndexing;
	uint32_t shaderSampledImageArrayDynamicIndexing;
	uint32_t shaderStorageBufferArrayDynamicIndexing;
	uint32_t shaderStorageImageArrayDynamicIndexing;
	uint32_t shaderClipDistance;
	uint32_t shaderCullDistance;
	uint32_t shaderFloat64;
	uint32_t shaderInt64;
	uint32_t shaderInt16;
	uint32_t shaderResourceResidency;
	uint32_t shaderResourceMinLod;
	uint32_t sparseBinding;
	uint32_t sparseResidencyBuffer;
	uint32_t sparseResidencyImage2D;
	uint32_t sparseResidencyImage3D;
	uint32_t sparseResidency2Samples;
	uint32_t sparseResidency4Samples;
	uint32_t sparseResidency8Samples;
	uint32_t sparseResidency16Samples;
	uint32_t sparseResidencyAliased;
	uint32_t variableMultisampleRate;
	uint32_t inheritedQueries;
} VkPhysicalDeviceFeatures;

typedef struct VkDeviceQueueCreateInfo {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	uint32_t queueFamilyIndex;
	uint32_t queueCount;
	const float* pQueuePriorities;
} VkDeviceQueueCreateInfo;

typedef struct VkDeviceCreateInfo {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	uint32_t queueCreateInfoCount;
	const VkDeviceQueueCreateInfo* pQueueCreateInfos;
	uint32_t enabledLayerCount;
	const char* const* ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char* const* ppEnabledExtensionNames;
	const VkPhysicalDeviceFeatures* pEnabledFeatures;
} VkDeviceCreateInfo;

typedef struct VkComponentMapping {
	uint32_t r, g, b, a;
} VkComponentMapping;

typedef struct VkImageSubresourceRange {
	VkFlags aspectMask;
	uint32_t baseMipLevel;
	uint32_t levelCount;
	uint32_t baseArrayLayer;
	uint32_t layerCount;
} VkImageSubresourceRange;

typedef struct VkImageViewCreateInfo {
	VkStructureType sType;
	const void* pNext;
	VkFlags flags;
	VkImage image;
	uint32_t viewType;
	VkFormat format;
	VkComponentMapping components;
	VkImageSubresourceRange subresourceRange;
} VkImageViewCreateInfo;

#endif // HAS_SYSTEM_VULKAN

// Libretro Vulkan constants & structures
#define RETRO_HW_CONTEXT_VULKAN 6
#define RETRO_HW_RENDER_INTERFACE_VULKAN 0
#define RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION 5
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN 0
#define RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION 2

#ifndef RETRO_ENVIRONMENT_EXPERIMENTAL
#define RETRO_ENVIRONMENT_EXPERIMENTAL 0x10000
#endif

#define RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE (41 | RETRO_ENVIRONMENT_EXPERIMENTAL)
#define RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE (43 | RETRO_ENVIRONMENT_EXPERIMENTAL)

struct retro_vulkan_image {
	VkImageView image_view;
	VkImageLayout image_layout;
	VkImageViewCreateInfo create_info;
};

typedef void (*retro_vulkan_set_image_t)(void* handle,
										 const struct retro_vulkan_image* image,
										 uint32_t num_semaphores,
										 const VkSemaphore* semaphores,
										 uint32_t src_queue_family);

typedef uint32_t (*retro_vulkan_get_sync_index_t)(void* handle);
typedef uint32_t (*retro_vulkan_get_sync_index_mask_t)(void* handle);
typedef void (*retro_vulkan_set_command_buffers_t)(void* handle,
												   uint32_t num_cmd,
												   const VkCommandBuffer* cmd);
typedef void (*retro_vulkan_wait_sync_index_t)(void* handle);
typedef void (*retro_vulkan_lock_queue_t)(void* handle);
typedef void (*retro_vulkan_unlock_queue_t)(void* handle);
typedef void (*retro_vulkan_set_signal_semaphore_t)(void* handle, VkSemaphore semaphore);
typedef const VkApplicationInfo* (*retro_vulkan_get_application_info_t)(void);

struct retro_vulkan_context {
	VkPhysicalDevice gpu;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family_index;
	VkQueue presentation_queue;
	uint32_t presentation_queue_family_index;
};

typedef bool (*retro_vulkan_create_device_t)(
	struct retro_vulkan_context* context,
	VkInstance instance,
	VkPhysicalDevice gpu,
	VkSurfaceKHR surface,
	PFN_vkGetInstanceProcAddr get_instance_proc_addr,
	const char** required_device_extensions,
	unsigned num_required_device_extensions,
	const char** required_device_layers,
	unsigned num_required_device_layers,
	const VkPhysicalDeviceFeatures* required_features);

typedef void (*retro_vulkan_destroy_device_t)(void);

typedef VkInstance (*retro_vulkan_create_instance_wrapper_t)(
	void* opaque, const VkInstanceCreateInfo* create_info);

typedef VkInstance (*retro_vulkan_create_instance_t)(
	PFN_vkGetInstanceProcAddr get_instance_proc_addr,
	const VkApplicationInfo* app,
	retro_vulkan_create_instance_wrapper_t create_instance_wrapper,
	void* opaque);

typedef VkDevice (*retro_vulkan_create_device_wrapper_t)(
	VkPhysicalDevice gpu, void* opaque,
	const VkDeviceCreateInfo* create_info);

typedef bool (*retro_vulkan_create_device2_t)(
	struct retro_vulkan_context* context,
	VkInstance instance,
	VkPhysicalDevice gpu,
	VkSurfaceKHR surface,
	PFN_vkGetInstanceProcAddr get_instance_proc_addr,
	retro_vulkan_create_device_wrapper_t create_device_wrapper,
	void* opaque);

struct retro_hw_render_context_negotiation_interface_vulkan {
	int interface_type;
	unsigned interface_version;
	retro_vulkan_get_application_info_t get_application_info;
	retro_vulkan_create_device_t create_device;
	retro_vulkan_destroy_device_t destroy_device;
	retro_vulkan_create_instance_t create_instance;
	retro_vulkan_create_device2_t create_device2;
};

struct retro_hw_render_interface_vulkan {
	int interface_type;
	unsigned interface_version;
	void* handle;
	VkInstance instance;
	VkPhysicalDevice gpu;
	VkDevice device;
	PFN_vkGetDeviceProcAddr get_device_proc_addr;
	PFN_vkGetInstanceProcAddr get_instance_proc_addr;
	VkQueue queue;
	unsigned queue_index;
	retro_vulkan_set_image_t set_image;
	retro_vulkan_get_sync_index_t get_sync_index;
	retro_vulkan_get_sync_index_mask_t get_sync_index_mask;
	retro_vulkan_wait_sync_index_t wait_sync_index;
	retro_vulkan_set_command_buffers_t set_command_buffers;
	retro_vulkan_lock_queue_t lock_queue;
	retro_vulkan_unlock_queue_t unlock_queue;
	retro_vulkan_set_signal_semaphore_t set_signal_semaphore;
};

#endif // VK_LIBRETRO_H
