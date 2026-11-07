#pragma once

#ifndef VULKANCONTEXT_H
#define VULKANCONTEXT_H

#include "ptpch.h"
#include <vulkan/vulkan.h>
#include <functional>

struct GLFWwindow;

namespace Lucerna {

	struct GpuBuffer {
		VkBuffer Buffer = VK_NULL_HANDLE;
		VkDeviceMemory Memory = VK_NULL_HANDLE;
		VkDeviceSize Size = 0;
		void* Mapped = nullptr;      // solo per i buffer host-visible
		VkDeviceAddress Address = 0; // solo con VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
	};

	struct GpuImage {
		VkImage Image = VK_NULL_HANDLE;
		VkDeviceMemory Memory = VK_NULL_HANDLE;
		VkImageView View = VK_NULL_HANDLE;
		VkFormat Format = VK_FORMAT_UNDEFINED;
		uint32_t Width = 0, Height = 0, Layers = 1, Levels = 1;
	};

	/*
	* Instance, device e swapchain, piu' il ciclo di vita del frame e la creazione di risorse.
	*
	* C'e' un solo frame in volo: BeginFrame aspetta la fine del precedente, quindi da li' in
	* poi nessuna risorsa e' in uso dalla GPU. Il path tracer accumula su immagini che
	* comunque si leggono frame dopo frame, quindi un secondo frame in volo non darebbe nulla.
	*/
	class VulkanContext {
	public:
		static VulkanContext& Get() { return *s_Instance; }

		bool Initialize(GLFWwindow* window, bool vsync);
		void Shutdown();

		// Ordine di un frame: BeginFrame, comandi compute su Cmd(), BeginUiPass, disegno ImGui,
		// EndUiPass, SubmitFrame, finestre secondarie di ImGui, PresentFrame.
		// BeginFrame ritorna false se il frame va saltato (finestra minimizzata, swapchain rifatta).
		bool BeginFrame();
		void BeginUiPass();
		void EndUiPass();
		void SubmitFrame();
		void PresentFrame();

		void SetVsync(bool vsync);
		VkCommandBuffer Cmd() const { return m_Cmd; }

		// Distrugge qualcosa dopo la fine del frame in corso: i comandi gia' registrati (o le
		// draw list di ImGui) possono ancora riferirlo.
		void Defer(std::function<void()> destroy) { m_Deferred.push_back(std::move(destroy)); }

		// Esegue comandi su un command buffer proprio e aspetta che finiscano.
		void Immediate(const std::function<void(VkCommandBuffer)>& record);

		GpuBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory);
		void DestroyBuffer(GpuBuffer& buffer);
		void UploadBuffer(const GpuBuffer& destination, const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

		GpuImage CreateImage(VkFormat format, uint32_t width, uint32_t height, VkImageUsageFlags usage,
		                     uint32_t layers = 1, uint32_t levels = 1, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
		// Le immagini di calcolo stanno sempre in GENERAL: bastano le barriere di memoria.
		void TransitionToGeneral(const GpuImage& image);
		void DestroyImage(GpuImage& image);

		VkInstance Instance() const { return m_Instance; }
		VkPhysicalDevice PhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice Device() const { return m_Device; }
		VkQueue Queue() const { return m_Queue; }
		uint32_t QueueFamily() const { return m_QueueFamily; }
		VkFormat SwapchainFormat() const { return m_SwapchainFormat; }
		uint32_t SwapchainImageCount() const { return static_cast<uint32_t>(m_SwapchainImages.size()); }
		const char* DeviceName() const { return m_DeviceProperties.deviceName; }
		float TimestampPeriod() const { return m_DeviceProperties.limits.timestampPeriod; }

	private:
		bool CreateInstance(bool validation);
		bool PickPhysicalDevice();
		bool CreateDevice();
		void CreateSwapchain();
		void DestroySwapchainViews();
		uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

	private:
		static VulkanContext* s_Instance;

		GLFWwindow* m_Window = nullptr;
		VkInstance m_Instance = VK_NULL_HANDLE;
		VkDebugUtilsMessengerEXT m_Messenger = VK_NULL_HANDLE;
		VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkPhysicalDeviceProperties m_DeviceProperties{};
		VkDevice m_Device = VK_NULL_HANDLE;
		VkQueue m_Queue = VK_NULL_HANDLE;
		uint32_t m_QueueFamily = 0;

		VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
		VkFormat m_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
		VkExtent2D m_SwapchainExtent{};
		std::vector<VkImage> m_SwapchainImages;
		std::vector<VkImageView> m_SwapchainViews;
		std::vector<VkSemaphore> m_RenderFinished; // uno per immagine: la presentazione li tiene occupati
		bool m_Vsync = true;
		bool m_SwapchainDirty = false;

		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		VkCommandBuffer m_Cmd = VK_NULL_HANDLE;
		VkCommandBuffer m_ImmediateCmd = VK_NULL_HANDLE;
		VkFence m_FrameFence = VK_NULL_HANDLE;
		VkFence m_ImmediateFence = VK_NULL_HANDLE;
		VkSemaphore m_ImageAvailable = VK_NULL_HANDLE;
		uint32_t m_ImageIndex = 0;
		std::vector<std::function<void()>> m_Deferred;
	};
}

#endif // VULKANCONTEXT_H
