#include "TextureArray.h"
#include <cmath>
#include <cstring>
#include "../stb_image.h"

namespace PathTracer {

	namespace {
		// Ricampionamento nearest-neighbour: tutti i layer di un'immagine array
		// devono avere la stessa dimensione, e le immagini sorgente non ce l'hanno.
		std::vector<unsigned char> Resample(const unsigned char* src, int srcW, int srcH, uint32_t dstSize) {
			std::vector<unsigned char> dst(static_cast<size_t>(dstSize) * dstSize * 4);

			for (uint32_t y = 0; y < dstSize; y++) {
				int sy = static_cast<int>((static_cast<float>(y) + 0.5f) / dstSize * srcH);
				sy = std::min(std::max(sy, 0), srcH - 1);

				for (uint32_t x = 0; x < dstSize; x++) {
					int sx = static_cast<int>((static_cast<float>(x) + 0.5f) / dstSize * srcW);
					sx = std::min(std::max(sx, 0), srcW - 1);

					const unsigned char* s = src + (static_cast<size_t>(sy) * srcW + sx) * 4;
					unsigned char* d = dst.data() + (static_cast<size_t>(y) * dstSize + x) * 4;
					d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
				}
			}

			return dst;
		}
	}

	void TextureArray::Release() {
		VulkanContext& vk = VulkanContext::Get();
		vk.DestroyImage(m_Image);
		if (m_Sampler != VK_NULL_HANDLE)
			vkDestroySampler(vk.Device(), m_Sampler, nullptr);
		m_Sampler = VK_NULL_HANDLE;
	}

	void TextureArray::Load(const std::vector<std::string>& paths, uint32_t size, VkFormat format) {
		VulkanContext& vk = VulkanContext::Get();
		Release();

		const uint32_t layers = std::max(static_cast<uint32_t>(paths.size()), 1u); // almeno un layer valido
		// Catena di mip completa: dopo il primo rimbalzo lo shader campiona un livello ridotto.
		const uint32_t levels = static_cast<uint32_t>(std::log2(size)) + 1;

		// R8G8B8A8_SRGB: le texture diffuse sono authored in spazio sRGB (gamma ~2.2). Con questo
		// formato il sampler le decodifica in lineare in hardware, cosi' il path tracer lavora
		// sempre in lineare. L'alpha resta lineare (non e' colore).
		m_Image = vk.CreateImage(format, size, size,
		                         VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		                         layers, levels, VK_IMAGE_VIEW_TYPE_2D_ARRAY);

		VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.maxLod = static_cast<float>(levels);
		vkCreateSampler(vk.Device(), &samplerInfo, nullptr, &m_Sampler);

		const size_t layerBytes = static_cast<size_t>(size) * size * 4;
		GpuBuffer staging = vk.CreateBuffer(layerBytes * layers, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		unsigned char* mapped = static_cast<unsigned char*>(staging.Mapped);
		memset(mapped, 255, layerBytes * layers); // bianco: scena senza texture o file illeggibile

		// l'origine delle UV in OBJ e' in basso a sinistra, quella di stb_image in alto
		stbi_set_flip_vertically_on_load(true);

		for (size_t layer = 0; layer < paths.size(); layer++) {
			int width = 0, height = 0, channels = 0;
			unsigned char* pixels = stbi_load(paths[layer].c_str(), &width, &height, &channels, 4);

			if (pixels == nullptr) {
				std::cerr << "Texture load failed: " << paths[layer] << " (" << stbi_failure_reason() << ")" << std::endl;
				continue;
			}

			const std::vector<unsigned char> resampled = Resample(pixels, width, height, size);
			memcpy(mapped + layer * layerBytes, resampled.data(), layerBytes);
			stbi_image_free(pixels);

			std::cout << "Texture loaded: " << paths[layer] << " (" << width << "x" << height
				<< " -> " << size << "x" << size << ", layer " << layer << ")" << std::endl;
		}

		stbi_set_flip_vertically_on_load(false);

		const VkImage image = m_Image.Image;
		vk.Immediate([&](VkCommandBuffer cmd) {
			auto barrier = [&](uint32_t level, uint32_t count, VkImageLayout from, VkImageLayout to,
			                   VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess) {
				VkImageMemoryBarrier2 b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
				b.srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
				b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				b.dstStageMask = dstStage;
				b.dstAccessMask = dstAccess;
				b.oldLayout = from;
				b.newLayout = to;
				b.image = image;
				b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, level, count, 0, layers };
				VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
				dependency.imageMemoryBarrierCount = 1;
				dependency.pImageMemoryBarriers = &b;
				vkCmdPipelineBarrier2(cmd, &dependency);
			};

			barrier(0, levels, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

			VkBufferImageCopy copy{};
			copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, layers };
			copy.imageExtent = { size, size, 1 };
			vkCmdCopyBufferToImage(cmd, staging.Buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

			// Ogni mip si ricava dal precedente. Il blit tra formati _SRGB filtra in lineare.
			int32_t mip = static_cast<int32_t>(size);
			for (uint32_t level = 1; level < levels; level++) {
				barrier(level - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				        VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

				VkImageBlit blit{};
				blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level - 1, 0, layers };
				blit.srcOffsets[1] = { mip, mip, 1 };
				mip = std::max(mip / 2, 1);
				blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, level, 0, layers };
				blit.dstOffsets[1] = { mip, mip, 1 };
				vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
			}

			if (levels > 1)
				barrier(0, levels - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
			barrier(levels - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
		});

		vk.DestroyBuffer(staging);
	}
}
