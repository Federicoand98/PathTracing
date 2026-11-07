#include "Vulkan/VulkanContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstring>

namespace Lucerna {

	VulkanContext* VulkanContext::s_Instance = nullptr;

	namespace {
		// "ERROR" e' lo stesso prefisso dei log GL: tools/render_check/capture.sh lo conta.
		VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
		                                             const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
			if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
				std::cout << "ERROR::VULKAN: " << data->pMessage << std::endl;
			else
				std::cout << "WARNING::VULKAN: " << data->pMessage << std::endl;
			return VK_FALSE;
		}

		bool HasLayer(const char* name) {
			uint32_t count = 0;
			vkEnumerateInstanceLayerProperties(&count, nullptr);
			std::vector<VkLayerProperties> layers(count);
			vkEnumerateInstanceLayerProperties(&count, layers.data());
			for (const VkLayerProperties& layer : layers)
				if (strcmp(layer.layerName, name) == 0)
					return true;
			return false;
		}

		const std::vector<const char*> DEVICE_EXTENSIONS = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // core in 1.3, ma il backend ImGui lo carica con il nome KHR
			VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
			VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
			VK_KHR_RAY_QUERY_EXTENSION_NAME,
		};

		bool HasDeviceExtensions(VkPhysicalDevice device) {
			uint32_t count = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
			std::vector<VkExtensionProperties> available(count);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
			for (const char* wanted : DEVICE_EXTENSIONS) {
				bool found = false;
				for (const VkExtensionProperties& extension : available)
					found = found || strcmp(extension.extensionName, wanted) == 0;
				if (!found)
					return false;
			}
			return true;
		}
	}

	bool VulkanContext::Initialize(GLFWwindow* window, bool vsync) {
		s_Instance = this;
		m_Window = window;
		m_Vsync = vsync;

		// Test/CI: PT_VALIDATION=1 attiva i validation layer anche nelle build Release.
	#ifdef NDEBUG
		const bool validation = getenv("PT_VALIDATION") != nullptr;
	#else
		const bool validation = true;
	#endif

		if (!CreateInstance(validation))
			return false;
		if (glfwCreateWindowSurface(m_Instance, window, nullptr, &m_Surface) != VK_SUCCESS) {
			std::cout << "ERROR::VULKAN: glfwCreateWindowSurface fallita" << std::endl;
			return false;
		}
		if (!PickPhysicalDevice() || !CreateDevice())
			return false;

		VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_QueueFamily;
		vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool);

		VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		allocInfo.commandPool = m_CommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		vkAllocateCommandBuffers(m_Device, &allocInfo, &m_Cmd);
		vkAllocateCommandBuffers(m_Device, &allocInfo, &m_ImmediateCmd);

		VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		vkCreateFence(m_Device, &fenceInfo, nullptr, &m_ImmediateFence);
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		vkCreateFence(m_Device, &fenceInfo, nullptr, &m_FrameFence);

		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailable);

		CreateSwapchain();

		std::cout << "Vulkan device: " << m_DeviceProperties.deviceName << " (API "
			<< VK_API_VERSION_MAJOR(m_DeviceProperties.apiVersion) << "."
			<< VK_API_VERSION_MINOR(m_DeviceProperties.apiVersion) << ")" << std::endl;
		return true;
	}

	bool VulkanContext::CreateInstance(bool validation) {
		VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
		appInfo.pApplicationName = "Lucerna";
		appInfo.apiVersion = VK_API_VERSION_1_3;

		uint32_t glfwCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwCount);
		if (glfwExtensions == nullptr) {
			std::cout << "ERROR::VULKAN: GLFW non trova un supporto Vulkan per le finestre" << std::endl;
			return false;
		}
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwCount);

		const char* validationLayer = "VK_LAYER_KHRONOS_validation";
		validation = validation && HasLayer(validationLayer);
		if (validation)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();
		if (validation) {
			createInfo.enabledLayerCount = 1;
			createInfo.ppEnabledLayerNames = &validationLayer;
		}

		if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
			std::cout << "ERROR::VULKAN: vkCreateInstance fallita" << std::endl;
			return false;
		}

		if (validation) {
			VkDebugUtilsMessengerCreateInfoEXT messengerInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
			messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			messengerInfo.pfnUserCallback = DebugCallback;
			auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
			create(m_Instance, &messengerInfo, nullptr, &m_Messenger);
			std::cout << "Vulkan validation layer attivo" << std::endl;
		}
		return true;
	}

	bool VulkanContext::PickPhysicalDevice() {
		uint32_t count = 0;
		vkEnumeratePhysicalDevices(m_Instance, &count, nullptr);
		std::vector<VkPhysicalDevice> devices(count);
		vkEnumeratePhysicalDevices(m_Instance, &count, devices.data());

		// Una GPU discreta vince sempre: llvmpipe supporta le stesse estensioni ma e' la CPU.
		int bestScore = -1;
		for (VkPhysicalDevice device : devices) {
			if (!HasDeviceExtensions(device))
				continue;

			uint32_t familyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
			std::vector<VkQueueFamilyProperties> families(familyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

			for (uint32_t i = 0; i < familyCount; i++) {
				VkBool32 present = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &present);
				const VkQueueFlags needed = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
				if (!present || (families[i].queueFlags & needed) != needed)
					continue;

				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(device, &properties);
				const int score = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2
				                : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1 : 0;
				if (score > bestScore) {
					bestScore = score;
					m_PhysicalDevice = device;
					m_QueueFamily = i;
					m_DeviceProperties = properties;
				}
				break;
			}
		}

		if (m_PhysicalDevice == VK_NULL_HANDLE)
			std::cout << "ERROR::VULKAN: nessuna GPU con ray query e presentazione" << std::endl;
		return m_PhysicalDevice != VK_NULL_HANDLE;
	}

	bool VulkanContext::CreateDevice() {
		const float priority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
		queueInfo.queueFamilyIndex = m_QueueFamily;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &priority;

		VkPhysicalDeviceRayQueryFeaturesKHR rayQuery{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
		rayQuery.rayQuery = VK_TRUE;

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
		accelerationStructure.pNext = &rayQuery;
		accelerationStructure.accelerationStructure = VK_TRUE;

		VkPhysicalDeviceVulkan13Features vulkan13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
		vulkan13.pNext = &accelerationStructure;
		vulkan13.dynamicRendering = VK_TRUE;
		vulkan13.synchronization2 = VK_TRUE;

		VkPhysicalDeviceVulkan12Features vulkan12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
		vulkan12.pNext = &vulkan13;
		vulkan12.bufferDeviceAddress = VK_TRUE;
		vulkan12.hostQueryReset = VK_TRUE;

		VkPhysicalDeviceFeatures2 features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
		features.pNext = &vulkan12;

		VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
		createInfo.pNext = &features;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pQueueCreateInfos = &queueInfo;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
		createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

		if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
			std::cout << "ERROR::VULKAN: vkCreateDevice fallita" << std::endl;
			return false;
		}
		vkGetDeviceQueue(m_Device, m_QueueFamily, 0, &m_Queue);
		return true;
	}

	void VulkanContext::DestroySwapchainViews() {
		for (VkImageView view : m_SwapchainViews)
			vkDestroyImageView(m_Device, view, nullptr);
		m_SwapchainViews.clear();
	}

	void VulkanContext::CreateSwapchain() {
		vkDeviceWaitIdle(m_Device);
		DestroySwapchainViews();

		VkSurfaceCapabilitiesKHR capabilities;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		m_SwapchainExtent = capabilities.currentExtent;
		if (m_SwapchainExtent.width == UINT32_MAX) {
			m_SwapchainExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			m_SwapchainExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
		}

		// Senza vsync si misura il costo reale delle passate: vedi PT_NOVSYNC.
		VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
		if (!m_Vsync) {
			uint32_t count = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, nullptr);
			std::vector<VkPresentModeKHR> modes(count);
			vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, modes.data());
			for (VkPresentModeKHR mode : modes)
				if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
					presentMode = mode;
		}

		uint32_t imageCount = capabilities.minImageCount + 1;
		if (capabilities.maxImageCount > 0)
			imageCount = std::min(imageCount, capabilities.maxImageCount);

		// L'encoding sRGB lo fa gia' il tonemap, e ImGui lavora con colori non lineari: un
		// formato _SRGB qui codificherebbe tutto due volte.
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> formats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());
		VkSurfaceFormatKHR surfaceFormat = formats[0];
		for (const VkSurfaceFormatKHR& format : formats)
			if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				surfaceFormat = format;
		m_SwapchainFormat = surfaceFormat.format;

		VkSwapchainKHR oldSwapchain = m_Swapchain;
		VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = m_SwapchainExtent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.preTransform = capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = oldSwapchain;
		vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain);
		if (oldSwapchain != VK_NULL_HANDLE)
			vkDestroySwapchainKHR(m_Device, oldSwapchain, nullptr);

		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
		m_SwapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

		m_SwapchainViews.resize(imageCount);
		for (uint32_t i = 0; i < imageCount; i++) {
			VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewInfo.image = m_SwapchainImages[i];
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = surfaceFormat.format;
			viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			vkCreateImageView(m_Device, &viewInfo, nullptr, &m_SwapchainViews[i]);
		}

		VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		while (m_RenderFinished.size() < imageCount) {
			VkSemaphore semaphore;
			vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &semaphore);
			m_RenderFinished.push_back(semaphore);
		}

		m_SwapchainDirty = false;
	}

	void VulkanContext::SetVsync(bool vsync) {
		m_Vsync = vsync;
		m_SwapchainDirty = true;
	}

	bool VulkanContext::BeginFrame() {
		vkWaitForFences(m_Device, 1, &m_FrameFence, VK_TRUE, UINT64_MAX);

		// Il frame precedente e' finito: quello che aspettava questo momento si puo' distruggere.
		for (const std::function<void()>& destroy : m_Deferred)
			destroy();
		m_Deferred.clear();

		int width = 0, height = 0;
		glfwGetFramebufferSize(m_Window, &width, &height);
		if (width == 0 || height == 0)
			return false;

		if (m_SwapchainDirty || static_cast<uint32_t>(width) != m_SwapchainExtent.width
			|| static_cast<uint32_t>(height) != m_SwapchainExtent.height)
			CreateSwapchain();

		VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAvailable, VK_NULL_HANDLE, &m_ImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			m_SwapchainDirty = true;
			return false;
		}

		vkResetFences(m_Device, 1, &m_FrameFence);
		vkResetCommandBuffer(m_Cmd, 0);

		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(m_Cmd, &beginInfo);
		return true;
	}

	void VulkanContext::BeginUiPass() {
		VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.image = m_SwapchainImages[m_ImageIndex];
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.imageMemoryBarrierCount = 1;
		dependency.pImageMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_Cmd, &dependency);

		VkRenderingAttachmentInfo attachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		attachment.imageView = m_SwapchainViews[m_ImageIndex];
		attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
		renderingInfo.renderArea.extent = m_SwapchainExtent;
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &attachment;
		vkCmdBeginRendering(m_Cmd, &renderingInfo);
	}

	void VulkanContext::EndUiPass() {
		vkCmdEndRendering(m_Cmd);

		VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier.image = m_SwapchainImages[m_ImageIndex];
		barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.imageMemoryBarrierCount = 1;
		dependency.pImageMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_Cmd, &dependency);
	}

	void VulkanContext::SubmitFrame() {
		vkEndCommandBuffer(m_Cmd);

		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_ImageAvailable;
		submitInfo.pWaitDstStageMask = &waitStage;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_Cmd;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_RenderFinished[m_ImageIndex];
		vkQueueSubmit(m_Queue, 1, &submitInfo, m_FrameFence);
	}

	void VulkanContext::PresentFrame() {
		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_RenderFinished[m_ImageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_Swapchain;
		presentInfo.pImageIndices = &m_ImageIndex;

		VkResult result = vkQueuePresentKHR(m_Queue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
			m_SwapchainDirty = true;
	}

	void VulkanContext::Immediate(const std::function<void(VkCommandBuffer)>& record) {
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(m_ImmediateCmd, &beginInfo);
		record(m_ImmediateCmd);

		// La fence sincronizza la CPU, non la memoria: le scritture vanno rese visibili ai comandi
		// che verranno dopo.
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(m_ImmediateCmd, &dependency);

		vkEndCommandBuffer(m_ImmediateCmd);

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &m_ImmediateCmd;
		vkQueueSubmit(m_Queue, 1, &submitInfo, m_ImmediateFence);
		vkWaitForFences(m_Device, 1, &m_ImmediateFence, VK_TRUE, UINT64_MAX);
		vkResetFences(m_Device, 1, &m_ImmediateFence);
		vkResetCommandBuffer(m_ImmediateCmd, 0);
	}

	uint32_t VulkanContext::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
		VkPhysicalDeviceMemoryProperties memory;
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memory);
		for (uint32_t i = 0; i < memory.memoryTypeCount; i++)
			if ((typeBits & (1u << i)) && (memory.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		std::cout << "ERROR::VULKAN: nessun tipo di memoria adatto" << std::endl;
		return 0;
	}

	GpuBuffer VulkanContext::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory) {
		GpuBuffer buffer;
		buffer.Size = size;

		VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		vkCreateBuffer(m_Device, &bufferInfo, nullptr, &buffer.Buffer);

		VkMemoryRequirements requirements;
		vkGetBufferMemoryRequirements(m_Device, buffer.Buffer, &requirements);

		VkMemoryAllocateFlagsInfo flagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
		flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &flagsInfo : nullptr;
		allocInfo.allocationSize = requirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, memory);
		if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &buffer.Memory) != VK_SUCCESS)
			std::cout << "ERROR::VULKAN: allocazione di " << size << " byte fallita" << std::endl;
		vkBindBufferMemory(m_Device, buffer.Buffer, buffer.Memory, 0);

		if (memory & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
			vkMapMemory(m_Device, buffer.Memory, 0, VK_WHOLE_SIZE, 0, &buffer.Mapped);

		if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
			VkBufferDeviceAddressInfo addressInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
			addressInfo.buffer = buffer.Buffer;
			buffer.Address = vkGetBufferDeviceAddress(m_Device, &addressInfo);
		}
		return buffer;
	}

	void VulkanContext::DestroyBuffer(GpuBuffer& buffer) {
		if (buffer.Buffer == VK_NULL_HANDLE)
			return;
		vkDestroyBuffer(m_Device, buffer.Buffer, nullptr);
		vkFreeMemory(m_Device, buffer.Memory, nullptr);
		buffer = GpuBuffer{};
	}

	void VulkanContext::UploadBuffer(const GpuBuffer& destination, const void* data, VkDeviceSize size, VkDeviceSize offset) {
		GpuBuffer staging = CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		memcpy(staging.Mapped, data, size);

		Immediate([&](VkCommandBuffer cmd) {
			VkBufferCopy copy{ 0, offset, size };
			vkCmdCopyBuffer(cmd, staging.Buffer, destination.Buffer, 1, &copy);
		});

		DestroyBuffer(staging);
	}

	GpuImage VulkanContext::CreateImage(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
	                                    uint32_t layers, uint32_t levels, VkImageViewType viewType) {
		GpuImage image;
		image.Format = format;
		image.Width = width;
		image.Height = height;
		image.Layers = layers;
		image.Levels = levels;

		VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.flags = viewType == VK_IMAGE_VIEW_TYPE_CUBE ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = format;
		imageInfo.extent = { width, height, 1 };
		imageInfo.mipLevels = levels;
		imageInfo.arrayLayers = layers;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vkCreateImage(m_Device, &imageInfo, nullptr, &image.Image);

		VkMemoryRequirements requirements;
		vkGetImageMemoryRequirements(m_Device, image.Image, &requirements);

		VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		allocInfo.allocationSize = requirements.size;
		allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &image.Memory) != VK_SUCCESS)
			std::cout << "ERROR::VULKAN: allocazione immagine fallita" << std::endl;
		vkBindImageMemory(m_Device, image.Image, image.Memory, 0);

		VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = image.Image;
		viewInfo.viewType = viewType;
		viewInfo.format = format;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, layers };
		vkCreateImageView(m_Device, &viewInfo, nullptr, &image.View);
		return image;
	}

	void VulkanContext::TransitionToGeneral(const GpuImage& image) {
		Immediate([&](VkCommandBuffer cmd) {
			VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barrier.image = image.Image;
			barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, image.Levels, 0, image.Layers };

			VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
			dependency.imageMemoryBarrierCount = 1;
			dependency.pImageMemoryBarriers = &barrier;
			vkCmdPipelineBarrier2(cmd, &dependency);
		});
	}

	void VulkanContext::DestroyImage(GpuImage& image) {
		if (image.Image == VK_NULL_HANDLE)
			return;
		vkDestroyImageView(m_Device, image.View, nullptr);
		vkDestroyImage(m_Device, image.Image, nullptr);
		vkFreeMemory(m_Device, image.Memory, nullptr);
		image = GpuImage{};
	}

	void VulkanContext::Shutdown() {
		if (m_Device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_Device);
			for (const std::function<void()>& destroy : m_Deferred)
				destroy();
			m_Deferred.clear();

			DestroySwapchainViews();
			vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
			for (VkSemaphore semaphore : m_RenderFinished)
				vkDestroySemaphore(m_Device, semaphore, nullptr);
			vkDestroySemaphore(m_Device, m_ImageAvailable, nullptr);
			vkDestroyFence(m_Device, m_FrameFence, nullptr);
			vkDestroyFence(m_Device, m_ImmediateFence, nullptr);
			vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
			vkDestroyDevice(m_Device, nullptr);
		}
		if (m_Instance != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
			if (m_Messenger != VK_NULL_HANDLE) {
				auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
				destroy(m_Instance, m_Messenger, nullptr);
			}
			vkDestroyInstance(m_Instance, nullptr);
		}
		m_Device = VK_NULL_HANDLE;
		m_Instance = VK_NULL_HANDLE;
	}
}
