#pragma once

#ifndef COMPUTEPIPELINE_H
#define COMPUTEPIPELINE_H

#include "VulkanContext.h"

namespace PathTracer {

	/*
	* Pipeline compute con i descrittori in push: si dichiarano a ogni dispatch, senza pool ne'
	* set da tenere allineati. E' cio' che rende banale il ping-pong: scambiare due immagini
	* significa solo passarle a binding diversi.
	* La posizione in `bindings` e' il numero di binding nello shader.
	*/
	class ComputePipeline {
	public:
		ComputePipeline(const std::string& spirvPath, const std::vector<VkDescriptorType>& bindings, uint32_t pushConstantSize = 0);
		~ComputePipeline();

		void Bind(VkCommandBuffer cmd) const;
		void PushConstants(VkCommandBuffer cmd, const void* data, uint32_t size) const;

		// I setter accumulano, Commit li invia. Le viste e i buffer devono vivere fino alla Commit.
		ComputePipeline& Image(uint32_t binding, const GpuImage& image);
		ComputePipeline& Sampler(uint32_t binding, const GpuImage& image, VkSampler sampler, VkImageLayout layout);
		ComputePipeline& Buffer(uint32_t binding, const GpuBuffer& buffer);
		ComputePipeline& AccelerationStructure(uint32_t binding, const VkAccelerationStructureKHR& structure);
		void Commit(VkCommandBuffer cmd);

	private:
		static constexpr int MAX_WRITES = 32;

		std::vector<VkDescriptorType> m_Bindings;
		VkDescriptorSetLayout m_SetLayout = VK_NULL_HANDLE;
		VkPipelineLayout m_Layout = VK_NULL_HANDLE;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		uint32_t m_PushConstantSize = 0;

		VkWriteDescriptorSet m_Writes[MAX_WRITES]{};
		VkDescriptorImageInfo m_ImageInfos[MAX_WRITES]{};
		VkDescriptorBufferInfo m_BufferInfos[MAX_WRITES]{};
		VkWriteDescriptorSetAccelerationStructureKHR m_StructureInfos[MAX_WRITES]{};
		uint32_t m_WriteCount = 0;
	};

	// Rende visibili ai dispatch successivi le scritture di quelli precedenti.
	void ComputeBarrier(VkCommandBuffer cmd);
}

#endif // COMPUTEPIPELINE_H
