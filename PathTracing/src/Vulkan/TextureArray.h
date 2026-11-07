#pragma once

#ifndef TEXTUREARRAY_H
#define TEXTUREARRAY_H

#include "ptpch.h"
#include "Vulkan/VulkanContext.h"

namespace Lucerna {

	/*
	* Un'immagine 2D array con una texture per layer.
	*
	* Perche' un array e non N sampler2D: nel path tracer l'indice della texture dipende
	* dal materiale colpito, che varia da raggio a raggio dentro la stessa invocazione.
	* Indicizzare un array di sampler con un indice non "dynamically uniform" e' undefined
	* behavior in GLSL; un sampler2DArray invece si indicizza liberamente sul layer.
	*
	* Tutti i layer devono avere la stessa dimensione: le immagini vengono ricampionate.
	*/
	class TextureArray {
	public:
		TextureArray() = default;
		~TextureArray() { Release(); }

		// Se paths e' vuoto crea comunque un layer bianco: lo shader campiona sempre.
		// format: SRGB per il colore, UNORM per i dati (normal map)
		void Load(const std::vector<std::string>& paths, uint32_t size = 1024, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
		void Release();

		const GpuImage& Image() const { return m_Image; }
		VkSampler Sampler() const { return m_Sampler; }

	private:
		GpuImage m_Image;
		VkSampler m_Sampler = VK_NULL_HANDLE;
	};
}

#endif // TEXTUREARRAY_H
