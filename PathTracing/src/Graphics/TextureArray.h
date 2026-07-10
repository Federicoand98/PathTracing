#pragma once

#ifndef TEXTUREARRAY_H
#define TEXTUREARRAY_H

#include "ptpch.h"
#include <GL/glew.h>

namespace PathTracer {

	/*
	* Un GL_TEXTURE_2D_ARRAY con una texture per layer.
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
		~TextureArray();

		// Se paths e' vuoto crea comunque un layer 1x1 bianco: campionare una texture
		// incompleta e' undefined behavior, e lo shader campiona sempre.
		void Load(const std::vector<std::string>& paths, uint32_t size = 1024);
		void AttachSampler(int unit) const { glBindTextureUnit(unit, m_Texture); }

		int GetLayerCount() const { return m_Layers; }

	private:
		void Release();

		GLuint m_Texture = 0;
		uint32_t m_Size = 0;
		int m_Layers = 0;
	};
}

#endif // TEXTUREARRAY_H
