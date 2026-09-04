#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "api.h"
#include "hud.h"
#include "libretro.h"
#include "render.h"
#include "viewport.h"
#include "vk_libretro.h"
#include "vk_shaders.h"

#define VK_MAX_SWAPCHAIN_IMAGES 2

typedef struct {
	void* lib;
	bool available;
	bool initialized;
	int dev_w;
	int dev_h;
	int scale_mode;
	int sharpness;
	int effect;
	double aspect;
	double fps;
	double cpu;
	double use;
	int show_debug;

	VkInstance instance;
	VkPhysicalDevice gpu;
	VkDevice device;
	VkQueue queue;
	uint32_t queue_family;
	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buffer[VK_MAX_SWAPCHAIN_IMAGES];
	VkFence fences[VK_MAX_SWAPCHAIN_IMAGES];

	VkRenderPass render_pass;
	VkPipelineLayout comp_layout;
	VkPipeline comp_pipeline;
	VkPipelineLayout menu_layout;
	VkPipeline menu_pipeline;
	VkSampler sampler_linear;
	VkSampler sampler_nearest;

	VkDescriptorSetLayout desc_layout;
	VkDescriptorPool desc_pool;
	VkDescriptorSet desc_set;

	VkBuffer vbo;
	VkDeviceMemory vbo_mem;

	uint32_t sync_index;
	struct retro_vulkan_image current_image;
	uint32_t num_wait_semaphores;
	VkSemaphore wait_semaphores[4];

	struct retro_hw_render_callback hw_render;
	struct retro_hw_render_interface_vulkan iface;
	struct retro_hw_render_context_negotiation_interface_vulkan neg_iface;
	bool has_neg_iface;
	bool core_owns_device;

	// Dynamic loader symbols
	PFN_vkGetInstanceProcAddr get_instance_proc_addr;
	PFN_vkGetDeviceProcAddr get_device_proc_addr;
	PFN_vkCreateInstance create_instance;
	PFN_vkDestroyInstance destroy_instance;
	PFN_vkEnumeratePhysicalDevices enumerate_physical_devices;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties get_physical_device_queue_family_properties;
	PFN_vkCreateDevice create_device;
	PFN_vkDestroyDevice destroy_device;
} vk_backend_t;

static vk_backend_t vk = {
	.dev_w = 640,
	.dev_h = 480,
	.scale_mode = 1,
	.sharpness = 0,
	.effect = 0,
	.aspect = 0.0,
};

static void vk_set_scaling(int mode) { vk.scale_mode = mode; }
static void vk_set_sharpness(int s) { vk.sharpness = s; }
static void vk_set_effect(int e) { vk.effect = e; }
static void vk_set_aspect(double a) { vk.aspect = a; }
static void vk_update_debug(double fps, double cpu, double use, int show_debug) {
	vk.fps = fps;
	vk.cpu = cpu;
	vk.use = use;
	vk.show_debug = show_debug;
}

// Dynamic library loader
static bool vk_load_driver(void) {
	if (vk.lib) return true;

	const char* libs[] = {
		"libvulkan.so.1",
		"libvulkan.so",
		"libMaliVulkan.so.1",
		NULL};

	for (int i = 0; libs[i]; i++) {
		vk.lib = dlopen(libs[i], RTLD_NOW | RTLD_LOCAL);
		if (vk.lib) break;
	}

	if (!vk.lib) return false;

	vk.get_instance_proc_addr = (PFN_vkGetInstanceProcAddr)dlsym(vk.lib, "vkGetInstanceProcAddr");
	if (!vk.get_instance_proc_addr) {
		dlclose(vk.lib);
		vk.lib = NULL;
		return false;
	}

	vk.available = true;
	return true;
}

static bool vk_init(int screen_w, int screen_h) {
	vk.dev_w = screen_w;
	vk.dev_h = screen_h;
	return vk_load_driver();
}

static void vk_cb_set_image(void* handle, const struct retro_vulkan_image* image,
							uint32_t num_semaphores, const VkSemaphore* semaphores,
							uint32_t src_queue_family) {
	(void)handle;
	(void)src_queue_family;
	if (image) {
		vk.current_image = *image;
	}
	vk.num_wait_semaphores = (num_semaphores > 4) ? 4 : num_semaphores;
	for (uint32_t i = 0; i < vk.num_wait_semaphores; i++) {
		vk.wait_semaphores[i] = semaphores[i];
	}
}

static uint32_t vk_cb_get_sync_index(void* handle) {
	(void)handle;
	return vk.sync_index;
}

static uint32_t vk_cb_get_sync_index_mask(void* handle) {
	(void)handle;
	return (1 << VK_MAX_SWAPCHAIN_IMAGES) - 1;
}

static void vk_cb_wait_sync_index(void* handle) {
	(void)handle;
	// Wait on current frame fence if present
}

static void vk_cb_set_command_buffers(void* handle, uint32_t num_cmd, const VkCommandBuffer* cmd) {
	(void)handle;
	(void)num_cmd;
	(void)cmd;
}

static void vk_cb_lock_queue(void* handle) { (void)handle; }
static void vk_cb_unlock_queue(void* handle) { (void)handle; }
static void vk_cb_set_signal_semaphore(void* handle, VkSemaphore semaphore) {
	(void)handle;
	(void)semaphore;
}

static VkInstance vk_create_instance_wrapper(void* opaque, const VkInstanceCreateInfo* create_info) {
	(void)opaque;
	if (!vk.create_instance) return VK_NULL_HANDLE;
	VkInstance inst = VK_NULL_HANDLE;
	if (vk.create_instance(create_info, NULL, &inst) != VK_SUCCESS) return VK_NULL_HANDLE;
	return inst;
}

static const VkApplicationInfo* vk_get_app_info(void) {
	if (vk.has_neg_iface && vk.neg_iface.get_application_info) {
		const VkApplicationInfo* info = vk.neg_iface.get_application_info();
		if (info) return info;
	}
	static const VkApplicationInfo default_app = {
		.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName = "minarch",
		.applicationVersion = 1,
		.pEngineName = "minarch",
		.engineVersion = 1,
		.apiVersion = VK_API_VERSION_1_0,
	};
	return &default_app;
}

static bool vk_create_instance(void) {
	if (vk.instance) return true;

	vk.create_instance = (PFN_vkCreateInstance)vk.get_instance_proc_addr(NULL, "vkCreateInstance");
	if (!vk.create_instance) return false;

	const VkApplicationInfo* app_info = vk_get_app_info();
	VkInstanceCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo = app_info,
	};

	if (vk.has_neg_iface && vk.neg_iface.create_instance) {
		vk.instance = vk.neg_iface.create_instance(
			vk.get_instance_proc_addr, app_info, vk_create_instance_wrapper, NULL);
	}
	if (!vk.instance && vk.create_instance(&info, NULL, &vk.instance) != VK_SUCCESS) {
		return false;
	}

	vk.destroy_instance = (PFN_vkDestroyInstance)vk.get_instance_proc_addr(vk.instance, "vkDestroyInstance");
	vk.enumerate_physical_devices = (PFN_vkEnumeratePhysicalDevices)vk.get_instance_proc_addr(vk.instance, "vkEnumeratePhysicalDevices");
	vk.get_physical_device_queue_family_properties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vk.get_instance_proc_addr(vk.instance, "vkGetPhysicalDeviceQueueFamilyProperties");
	vk.create_device = (PFN_vkCreateDevice)vk.get_instance_proc_addr(vk.instance, "vkCreateDevice");
	vk.destroy_device = (PFN_vkDestroyDevice)vk.get_instance_proc_addr(vk.instance, "vkDestroyDevice");
	vk.get_device_proc_addr = (PFN_vkGetDeviceProcAddr)vk.get_instance_proc_addr(vk.instance, "vkGetDeviceProcAddr");
	return true;
}

static bool vk_select_physical_device(void) {
	if (vk.gpu) return true;
	if (!vk.enumerate_physical_devices) return false;

	uint32_t count = 0;
	if (vk.enumerate_physical_devices(vk.instance, &count, NULL) != VK_SUCCESS || count == 0) {
		return false;
	}

	VkPhysicalDevice gpus[4];
	uint32_t max_gpus = (count > 4) ? 4 : count;
	if (vk.enumerate_physical_devices(vk.instance, &max_gpus, gpus) != VK_SUCCESS || max_gpus == 0) {
		return false;
	}
	vk.gpu = gpus[0];
	return true;
}

static void vk_find_queue_family(void) {
	if (!vk.get_physical_device_queue_family_properties || !vk.gpu) return;

	uint32_t count = 0;
	vk.get_physical_device_queue_family_properties(vk.gpu, &count, NULL);
	if (count == 0) return;

	VkQueueFamilyProperties props[4];
	uint32_t max_props = (count > 4) ? 4 : count;
	vk.get_physical_device_queue_family_properties(vk.gpu, &max_props, props);

	for (uint32_t i = 0; i < max_props; i++) {
		if (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			vk.queue_family = i;
			break;
		}
	}
}

static bool vk_create_fallback_device(void) {
	if (!vk.create_device || !vk.gpu) return false;

	float priority = 1.0f;
	VkDeviceQueueCreateInfo q_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.queueFamilyIndex = vk.queue_family,
		.queueCount = 1,
		.pQueuePriorities = &priority,
	};
	VkDeviceCreateInfo dev_info = {
		.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos = &q_info,
	};

	if (vk.create_device(vk.gpu, &dev_info, NULL, &vk.device) != VK_SUCCESS) {
		return false;
	}
	vk.core_owns_device = false;
	return true;
}

static VkDevice vk_create_device_wrapper(VkPhysicalDevice gpu, void* opaque, const VkDeviceCreateInfo* create_info) {
	(void)opaque;
	if (!vk.create_device || !gpu) return VK_NULL_HANDLE;
	VkDevice dev = VK_NULL_HANDLE;
	if (vk.create_device(gpu, create_info, NULL, &dev) != VK_SUCCESS) return VK_NULL_HANDLE;
	return dev;
}

static bool vk_try_negotiate_device(void) {
	if (!vk.has_neg_iface) return false;
	struct retro_vulkan_context ctx = { .gpu = vk.gpu };

	if (vk.neg_iface.create_device2 &&
		vk.neg_iface.create_device2(&ctx, vk.instance, vk.gpu, VK_NULL_HANDLE,
									vk.get_instance_proc_addr, vk_create_device_wrapper, NULL)) {
		vk.device = ctx.device;
		vk.queue = ctx.queue;
		vk.queue_family = ctx.queue_family_index;
		if (ctx.gpu) vk.gpu = ctx.gpu;
		vk.core_owns_device = true;
		return true;
	}
	if (vk.neg_iface.create_device &&
		vk.neg_iface.create_device(&ctx, vk.instance, vk.gpu, VK_NULL_HANDLE,
								   vk.get_instance_proc_addr, NULL, 0, NULL, 0, NULL)) {
		vk.device = ctx.device;
		vk.queue = ctx.queue;
		vk.queue_family = ctx.queue_family_index;
		if (ctx.gpu) vk.gpu = ctx.gpu;
		vk.core_owns_device = true;
		return true;
	}
	return false;
}

static bool vk_create_device(void) {
	if (vk.device) return true;
	if (vk_try_negotiate_device()) return true;
	if (!vk_create_fallback_device()) return false;

	PFN_vkGetDeviceQueue get_queue = (PFN_vkGetDeviceQueue)vk.get_instance_proc_addr(vk.instance, "vkGetDeviceQueue");
	if (get_queue) {
		get_queue(vk.device, vk.queue_family, 0, &vk.queue);
	}
	return (vk.device != VK_NULL_HANDLE);
}

static void vk_populate_iface(void) {
	vk.iface.interface_type = RETRO_HW_RENDER_INTERFACE_VULKAN;
	vk.iface.interface_version = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
	vk.iface.handle = &vk;
	vk.iface.instance = vk.instance;
	vk.iface.gpu = vk.gpu;
	vk.iface.device = vk.device;
	vk.iface.queue = vk.queue;
	vk.iface.queue_index = vk.queue_family;
	vk.iface.get_instance_proc_addr = vk.get_instance_proc_addr;
	vk.iface.get_device_proc_addr = vk.get_device_proc_addr;
	vk.iface.set_image = vk_cb_set_image;
	vk.iface.get_sync_index = vk_cb_get_sync_index;
	vk.iface.get_sync_index_mask = vk_cb_get_sync_index_mask;
	vk.iface.wait_sync_index = vk_cb_wait_sync_index;
	vk.iface.set_command_buffers = vk_cb_set_command_buffers;
	vk.iface.lock_queue = vk_cb_lock_queue;
	vk.iface.unlock_queue = vk_cb_unlock_queue;
	vk.iface.set_signal_semaphore = vk_cb_set_signal_semaphore;
}

static bool vk_set_hw_render(struct retro_hw_render_callback* cb) {
	if (!cb || cb->context_type != RETRO_HW_CONTEXT_VULKAN) return false;
	vk.hw_render = *cb;
	cb->get_proc_address = (retro_hw_get_proc_address_t)vk.get_instance_proc_addr;
	return true;
}

static bool vk_get_hw_render_interface(void* data) {
	const struct retro_hw_render_interface** out_iface =
		(const struct retro_hw_render_interface**)data;
	if (!out_iface) return false;
	if (!vk.instance) vk_create_instance();
	if (!vk.gpu) vk_select_physical_device();
	if (!vk.device) vk_create_device();
	vk_populate_iface();
	*out_iface = (const struct retro_hw_render_interface*)&vk.iface;
	return true;
}

static bool vk_handle_environ(unsigned cmd, void* data) {
	if (!vk.lib && !vk_load_driver()) return false;

	if (cmd == RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER) {
		return true;
	}

	if (cmd == RETRO_ENVIRONMENT_SET_HW_RENDER) {
		return vk_set_hw_render((struct retro_hw_render_callback*)data);
	}

	if (cmd == RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE) {
		struct retro_hw_render_context_negotiation_interface_vulkan* neg =
			(struct retro_hw_render_context_negotiation_interface_vulkan*)data;
		if (!neg) return false;
		vk.neg_iface = *neg;
		vk.has_neg_iface = true;
		return true;
	}

	if (cmd == RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE) {
		return vk_get_hw_render_interface(data);
	}

	return false;
}

static void vk_post_core_load(unsigned initial_w, unsigned initial_h) {
	(void)initial_w;
	(void)initial_h;
	if (!vk_create_instance() || !vk_select_physical_device()) {
		LOG_error("vk: failed to initialize Vulkan instance/device\n");
		return;
	}
	vk_find_queue_family();
	if (!vk_create_device()) {
		LOG_error("vk: failed to create Vulkan logical device\n");
		return;
	}
	vk_populate_iface();
	if (vk.hw_render.context_reset) {
		vk.hw_render.context_reset();
	}
	vk.initialized = true;
}

static void vk_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch) {
	(void)data;
	(void)pitch;
	render_viewport_t vp;
	RENDER_computeViewport(width, height, vk.aspect, vk.scale_mode,
						   vk.dev_w, vk.dev_h, 0, false, &vp);

	// Advance frame sync index
	vk.sync_index = (vk.sync_index + 1) % VK_MAX_SWAPCHAIN_IMAGES;
}

static void vk_draw_menu(SDL_Surface* surface) {
	(void)surface;
}

static void vk_post_menu(void) {
	// Re-bind core state after menu
}

static void vk_flip(SDL_Surface* screen) {
	(void)screen;
}

static SDL_Surface* vk_capture_surface(void) {
	SDL_Surface* surf = SDL_CreateRGBSurface(
		0, vk.dev_w, vk.dev_h, 32,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	return surf;
}

static void vk_destroy_device_handle(void) {
	if (vk.core_owns_device && vk.has_neg_iface && vk.neg_iface.destroy_device) {
		vk.neg_iface.destroy_device();
	} else if (vk.device && vk.destroy_device) {
		vk.destroy_device(vk.device, NULL);
	}
	vk.device = VK_NULL_HANDLE;
	vk.queue = VK_NULL_HANDLE;
}

static void vk_quit(void) {
	if (vk.hw_render.context_destroy) {
		vk.hw_render.context_destroy();
	}
	vk_destroy_device_handle();
	if (vk.instance && vk.destroy_instance) {
		vk.destroy_instance(vk.instance, NULL);
		vk.instance = VK_NULL_HANDLE;
	}
	if (vk.lib) {
		dlclose(vk.lib);
		vk.lib = NULL;
	}
	vk.available = false;
	vk.initialized = false;
	vk.core_owns_device = false;
}

render_backend_ops_t render_vk_ops = {
	.type = RENDER_BACKEND_VK,
	.name = "vulkan",
	.init = vk_init,
	.handle_environ = vk_handle_environ,
	.post_core_load = vk_post_core_load,
	.video_refresh = vk_video_refresh,
	.draw_menu = vk_draw_menu,
	.post_menu = vk_post_menu,
	.flip = vk_flip,
	.capture_surface = vk_capture_surface,
	.set_scaling = vk_set_scaling,
	.set_sharpness = vk_set_sharpness,
	.set_effect = vk_set_effect,
	.set_aspect = vk_set_aspect,
	.update_debug = vk_update_debug,
	.quit = vk_quit,
};
