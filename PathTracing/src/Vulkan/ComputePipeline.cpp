#include "ComputePipeline.h"

namespace PathTracer {

	ComputePipeline::ComputePipeline(const std::string& spirvPath, const std::vector<VkDescriptorType>& bindings, uint32_t pushConstantSize)
		: m_Bindings(bindings), m_PushConstantSize(pushConstantSize) {
		VkDevice device = VulkanContext::Get().Device();

		std::ifstream file(spirvPath, std::ios::binary | std::ios::ate);
		if (!file) {
			std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << spirvPath << std::endl;
			return;
		}
		std::vector<uint32_t> code(static_cast<size_t>(file.tellg()) / sizeof(uint32_t));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(code.data()), code.size() * sizeof(uint32_t));

		VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		moduleInfo.codeSize = code.size() * sizeof(uint32_t);
		moduleInfo.pCode = code.data();
		VkShaderModule module;
		vkCreateShaderModule(device, &moduleInfo, nullptr, &module);

		std::vector<VkDescriptorSetLayoutBinding> layoutBindings(bindings.size());
		for (uint32_t i = 0; i < bindings.size(); i++)
			layoutBindings[i] = { i, bindings[i], 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

		VkDescriptorSetLayoutCreateInfo setInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		setInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
		setInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
		setInfo.pBindings = layoutBindings.data();
		vkCreateDescriptorSetLayout(device, &setInfo, nullptr, &m_SetLayout);

		VkPushConstantRange pushRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantSize };
		VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &m_SetLayout;
		layoutInfo.pushConstantRangeCount = pushConstantSize > 0 ? 1 : 0;
		layoutInfo.pPushConstantRanges = &pushRange;
		vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_Layout);

		VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
		pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		pipelineInfo.stage.module = module;
		pipelineInfo.stage.pName = "main";
		pipelineInfo.layout = m_Layout;
		if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
			std::cout << "ERROR::VULKAN: creazione pipeline fallita: " << spirvPath << std::endl;

		vkDestroyShaderModule(device, module, nullptr);
	}

	ComputePipeline::~ComputePipeline() {
		VkDevice device = VulkanContext::Get().Device();
		vkDestroyPipeline(device, m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device, m_Layout, nullptr);
		vkDestroyDescriptorSetLayout(device, m_SetLayout, nullptr);
	}

	void ComputePipeline::Bind(VkCommandBuffer cmd) const {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Pipeline);
	}

	void ComputePipeline::PushConstants(VkCommandBuffer cmd, const void* data, uint32_t size) const {
		vkCmdPushConstants(cmd, m_Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, size, data);
	}

	ComputePipeline& ComputePipeline::Image(uint32_t binding, const GpuImage& image) {
		m_ImageInfos[m_WriteCount] = { VK_NULL_HANDLE, image.View, VK_IMAGE_LAYOUT_GENERAL };
		m_Writes[m_WriteCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		m_Writes[m_WriteCount].dstBinding = binding;
		m_Writes[m_WriteCount].descriptorCount = 1;
		m_Writes[m_WriteCount].descriptorType = m_Bindings[binding];
		m_Writes[m_WriteCount].pImageInfo = &m_ImageInfos[m_WriteCount];
		m_WriteCount++;
		return *this;
	}

	ComputePipeline& ComputePipeline::Sampler(uint32_t binding, const GpuImage& image, VkSampler sampler, VkImageLayout layout) {
		m_ImageInfos[m_WriteCount] = { sampler, image.View, layout };
		m_Writes[m_WriteCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		m_Writes[m_WriteCount].dstBinding = binding;
		m_Writes[m_WriteCount].descriptorCount = 1;
		m_Writes[m_WriteCount].descriptorType = m_Bindings[binding];
		m_Writes[m_WriteCount].pImageInfo = &m_ImageInfos[m_WriteCount];
		m_WriteCount++;
		return *this;
	}

	ComputePipeline& ComputePipeline::Buffer(uint32_t binding, const GpuBuffer& buffer) {
		m_BufferInfos[m_WriteCount] = { buffer.Buffer, 0, VK_WHOLE_SIZE };
		m_Writes[m_WriteCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		m_Writes[m_WriteCount].dstBinding = binding;
		m_Writes[m_WriteCount].descriptorCount = 1;
		m_Writes[m_WriteCount].descriptorType = m_Bindings[binding];
		m_Writes[m_WriteCount].pBufferInfo = &m_BufferInfos[m_WriteCount];
		m_WriteCount++;
		return *this;
	}

	ComputePipeline& ComputePipeline::AccelerationStructure(uint32_t binding, const VkAccelerationStructureKHR& structure) {
		m_StructureInfos[m_WriteCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		m_StructureInfos[m_WriteCount].accelerationStructureCount = 1;
		m_StructureInfos[m_WriteCount].pAccelerationStructures = &structure;
		m_Writes[m_WriteCount] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		m_Writes[m_WriteCount].pNext = &m_StructureInfos[m_WriteCount];
		m_Writes[m_WriteCount].dstBinding = binding;
		m_Writes[m_WriteCount].descriptorCount = 1;
		m_Writes[m_WriteCount].descriptorType = m_Bindings[binding];
		m_WriteCount++;
		return *this;
	}

	void ComputePipeline::Commit(VkCommandBuffer cmd) {
		static const auto push = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(
			vkGetDeviceProcAddr(VulkanContext::Get().Device(), "vkCmdPushDescriptorSetKHR"));
		push(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_Layout, 0, m_WriteCount, m_Writes);
		m_WriteCount = 0;
	}

	void ComputeBarrier(VkCommandBuffer cmd) {
		VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
		barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;

		VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.memoryBarrierCount = 1;
		dependency.pMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmd, &dependency);
	}
}
