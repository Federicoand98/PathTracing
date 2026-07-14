#include "TextureArray.h"
#include "../stb_image.h"

namespace PathTracer {

	namespace {
		// Ricampionamento nearest-neighbour: tutti i layer di un GL_TEXTURE_2D_ARRAY
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

	TextureArray::~TextureArray() {
		Release();
	}

	void TextureArray::Release() {
		if (m_Texture != 0) {
			glDeleteTextures(1, &m_Texture);
			m_Texture = 0;
		}
	}

	void TextureArray::Load(const std::vector<std::string>& paths, uint32_t size) {
		Release();

		m_Size = size;
		m_Layers = std::max(static_cast<int>(paths.size()), 1); // almeno un layer valido

		glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_Texture);
		// GL_SRGB8_ALPHA8: le texture diffuse sono authored in spazio sRGB (gamma ~2.2).
		// Con questo formato interno il sampler le decodifica in lineare in hardware, cosi'
		// il path tracer lavora sempre in lineare. L'alpha resta lineare (non e' colore).
		// Il tonemap finale (fScreenQuad.frag) fa la ri-codifica lineare->sRGB in uscita.
		glTextureStorage3D(m_Texture, 1, GL_SRGB8_ALPHA8, m_Size, m_Size, m_Layers);

		glTextureParameteri(m_Texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTextureParameteri(m_Texture, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTextureParameteri(m_Texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(m_Texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (paths.empty()) {
			// layer bianco: il materiale senza texture moltiplica per 1
			const std::vector<unsigned char> white(static_cast<size_t>(m_Size) * m_Size * 4, 255);
			glTextureSubImage3D(m_Texture, 0, 0, 0, 0, m_Size, m_Size, 1, GL_RGBA, GL_UNSIGNED_BYTE, white.data());
			return;
		}

		// l'origine delle UV in OBJ e' in basso a sinistra, quella di stb_image in alto
		stbi_set_flip_vertically_on_load(true);

		for (size_t layer = 0; layer < paths.size(); layer++) {
			int width = 0, height = 0, channels = 0;
			unsigned char* pixels = stbi_load(paths[layer].c_str(), &width, &height, &channels, 4);

			if (pixels == nullptr) {
				std::cerr << "Texture load failed: " << paths[layer] << " (" << stbi_failure_reason() << ")" << std::endl;

				const std::vector<unsigned char> magenta(static_cast<size_t>(m_Size) * m_Size * 4, 255);
				glTextureSubImage3D(m_Texture, 0, 0, 0, (GLint)layer, m_Size, m_Size, 1, GL_RGBA, GL_UNSIGNED_BYTE, magenta.data());
				continue;
			}

			const std::vector<unsigned char> resampled = Resample(pixels, width, height, m_Size);
			glTextureSubImage3D(m_Texture, 0, 0, 0, (GLint)layer, m_Size, m_Size, 1, GL_RGBA, GL_UNSIGNED_BYTE, resampled.data());

			stbi_image_free(pixels);

			std::cout << "Texture loaded: " << paths[layer] << " (" << width << "x" << height
				<< " -> " << m_Size << "x" << m_Size << ", layer " << layer << ")" << std::endl;
		}

		stbi_set_flip_vertically_on_load(false);
	}
}
