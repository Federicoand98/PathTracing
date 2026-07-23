// Import di scene glTF 2.0 (interop Blender, ADR 0003). Definisce World::LoadGltf a parte per
// tenere la dipendenza cgltf fuori da World.cpp. Il mapping rispecchia il percorso OBJ:
// una MeshInfo per primitive (triangoli in LOCAL space, transform del nodo appiattita, un BLAS
// per mesh), e pbrMetallicRoughness -> Material come MaterialFromMtl fa con l'MTL.

#include "World.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>

#include "cgltf.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace PathTracer {

	namespace {

		// glTF codifica la texture URI come URL (spazi = %20, ...). cgltf la decodifica in-place.
		// Ritorna il path assoluto risolto rispetto alla cartella del .gltf.
		std::string ResolveTextureUri(const std::string& gltfDir, const cgltf_image* image) {
			if (!image || !image->uri || image->uri[0] == '\0')
				return {}; // embedded (GLB/data URI): non gestito, serve export non-binary (ADR 0003)

			std::string uri = image->uri;
			// niente data: URI (immagini base64 embedded)
			if (uri.rfind("data:", 0) == 0)
				return {};

			std::vector<char> buf(uri.begin(), uri.end());
			buf.push_back('\0');
			cgltf_decode_uri(buf.data());
			std::filesystem::path p = std::filesystem::path(gltfDir) / buf.data();
			return p.lexically_normal().string();
		}

		// pbrMetallicRoughness -> Material dell'engine. La registrazione della texture (privata
		// del World) la fa il chiamante: qui il path della base color texture esce da baseTexOut.
		Material MaterialFromGltf(const cgltf_material& gm, const std::string& gltfDir, std::string& baseTexOut) {
			Material m;
			baseTexOut.clear();

			glm::vec4 baseColor(1.0f);
			const cgltf_texture* baseTex = nullptr;
			float metallic = 0.0f, roughness = 1.0f;

			if (gm.has_pbr_metallic_roughness) {
				const auto& pbr = gm.pbr_metallic_roughness;
				baseColor = glm::make_vec4(pbr.base_color_factor);
				metallic = pbr.metallic_factor;
				roughness = pbr.roughness_factor;
				baseTex = pbr.base_color_texture.texture;
			}

			m.Color = baseColor;
			m.Metalness = glm::clamp(metallic, 0.0f, 1.0f);
			m.Roughness = glm::clamp(roughness, 0.0f, 1.0f);
			m.RefractionRoughness = m.Roughness;
			m.SpecularProbability = 0.04f;     // F0 dielettrico standard
			m.SpecularColor = glm::vec4(1.0f); // riflesso dielettrico non tinto
			m.RefractionColor = m.Color;

			// IOR e transmission stanno in estensioni glTF che cgltf espone come flag dedicati.
			if (gm.has_ior)
				m.RefractionRatio = glm::clamp(gm.ior.ior, 1.0f, 3.0f);
			if (gm.has_transmission)
				m.RefractionProbability = glm::clamp(gm.transmission.transmission_factor, 0.0f, 1.0f);

			// emissione: la magnitudine diventa la forza, la direzione il colore (come l'MTL Ke).
			glm::vec3 emissive = glm::make_vec3(gm.emissive_factor);
			if (gm.has_emissive_strength)
				emissive *= gm.emissive_strength.emissive_strength;
			float ke = glm::max(emissive.x, glm::max(emissive.y, emissive.z));
			if (ke > 0.0f) {
				m.EmissiveColor = glm::vec4(emissive / ke, 1.0f);
				m.EmissiveStrenght = ke;
			}

			if (baseTex && baseTex->image)
				baseTexOut = ResolveTextureUri(gltfDir, baseTex->image);
			return m;
		}

	} // namespace

	bool World::LoadGltf(const std::string& path) {
		cgltf_options options = {};
		cgltf_data* data = nullptr;

		if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
			std::cerr << "LoadGltf: parse fallito: " << path << std::endl;
			return false;
		}
		// carica i buffer binari (.bin / data URI) referenziati dagli accessor
		if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
			std::cerr << "LoadGltf: load buffers fallito: " << path << std::endl;
			cgltf_free(data);
			return false;
		}

		// da qui in poi si rimpiazza la scena: prima di toccare il World, tutto e' andato a buon fine
		DestroyScene();

		// In questo engine i raggi che mancano la geometria ritornano BackgroundColor come
		// radianza (con Environment Mapping ON; vedi PathTracing.comp): e' la sola luce di una
		// scena glTF, che a questo stadio non importa lampade. Cielo azzurro = illuminazione base.
		// Senza, la scena resta NERA (sfondo e oggetti). Non importiamo ancora le luci glTF.
		BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);

		const std::string gltfDir = std::filesystem::path(path).parent_path().string();

		// Materiali: appesi in blocco, con una mappa puntatore->indice globale per i primitive.
		std::unordered_map<const cgltf_material*, int> matIndex;
		for (size_t i = 0; i < data->materials_count; i++) {
			const cgltf_material& gm = data->materials[i];
			matIndex[&gm] = static_cast<int>(Materials.size());
			std::string baseTex;
			Material m = MaterialFromGltf(gm, gltfDir, baseTex);
			if (!baseTex.empty()) {
				m.AlbedoTexture = static_cast<float>(RegisterTexture(baseTex));
				m.Color = glm::vec4(1.0f); // la texture porta il colore, non tingerla due volte
			}
			Materials.push_back(m);
		}

		size_t triTotal = 0;

		// Nodi: si visita l'intera gerarchia. cgltf_node_transform_world appiattisce la catena
		// di transform dalla radice. Ogni primitive di ogni mesh referenziata diventa una MeshInfo.
		for (size_t n = 0; n < data->nodes_count; n++) {
			const cgltf_node& node = data->nodes[n];
			if (!node.mesh)
				continue;

			float world[16];
			cgltf_node_transform_world(&node, world);
			glm::mat4 transform = glm::make_mat4(world);

			for (size_t p = 0; p < node.mesh->primitives_count; p++) {
				const cgltf_primitive& prim = node.mesh->primitives[p];
				if (prim.type != cgltf_primitive_type_triangles)
					continue;

				// accessor delle attributi
				const cgltf_accessor* posA = nullptr;
				const cgltf_accessor* nrmA = nullptr;
				const cgltf_accessor* uvA = nullptr;
				for (size_t a = 0; a < prim.attributes_count; a++) {
					const cgltf_attribute& attr = prim.attributes[a];
					if (attr.type == cgltf_attribute_type_position) posA = attr.data;
					else if (attr.type == cgltf_attribute_type_normal) nrmA = attr.data;
					else if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvA = attr.data;
				}
				if (!posA)
					continue;

				const size_t vertCount = posA->count;
				std::vector<glm::vec3> pos(vertCount), nrm(vertCount);
				std::vector<glm::vec2> uv(vertCount, glm::vec2(0.0f));
				for (size_t v = 0; v < vertCount; v++) {
					cgltf_accessor_read_float(posA, v, glm::value_ptr(pos[v]), 3);
					if (nrmA) cgltf_accessor_read_float(nrmA, v, glm::value_ptr(nrm[v]), 3);
					if (uvA)  cgltf_accessor_read_float(uvA, v, glm::value_ptr(uv[v]), 2);
				}

				// indici: se assenti, la primitive e' non indicizzata (0,1,2,3,...)
				const size_t idxCount = prim.indices ? prim.indices->count : vertCount;
				auto idxAt = [&](size_t i) -> cgltf_size {
					return prim.indices ? cgltf_accessor_read_index(prim.indices, i) : i;
				};

				const int meshMat = prim.material && matIndex.count(prim.material)
					? matIndex[prim.material] : -1;

				MeshInfo mesh;
				mesh.FirstTriangle = static_cast<float>(Triangles.size());
				mesh.MaterialIndex = static_cast<float>(meshMat); // una primitive = un materiale

				glm::vec4 localMin(1e30f), localMax(-1e30f);
				size_t triCount = 0;

				for (size_t i = 0; i + 3 <= idxCount; i += 3) {
					const cgltf_size i0 = idxAt(i), i1 = idxAt(i + 1), i2 = idxAt(i + 2);

					Triangle tri;
					tri.A = glm::vec4(pos[i0], 1.0f);
					tri.B = glm::vec4(pos[i1], 1.0f);
					tri.C = glm::vec4(pos[i2], 1.0f);

					if (nrmA) {
						tri.NormalA = glm::vec4(nrm[i0], 0.0f);
						tri.NormalB = glm::vec4(nrm[i1], 0.0f);
						tri.NormalC = glm::vec4(nrm[i2], 0.0f);
					} else {
						// normale di faccia se il glTF non ne porta (raro, ma legale)
						glm::vec3 fn = glm::normalize(glm::cross(pos[i1] - pos[i0], pos[i2] - pos[i0]));
						tri.NormalA = tri.NormalB = tri.NormalC = glm::vec4(fn, 0.0f);
					}

					tri.UVA = uv[i0]; tri.UVB = uv[i1]; tri.UVC = uv[i2];
					tri.MaterialIndex = -1.0f; // eredita il materiale della mesh

					localMin = glm::min(localMin, glm::min(tri.A, glm::min(tri.B, tri.C)));
					localMax = glm::max(localMax, glm::max(tri.A, glm::max(tri.B, tri.C)));

					Triangles.push_back(tri);
					triCount++;
				}

				if (triCount == 0)
					continue;

				mesh.NumTriangles = static_cast<float>(triCount);
				mesh.LocalMin = localMin;
				mesh.LocalMax = localMax;

				Meshes.push_back(mesh);
				MeshNames.push_back(node.mesh->name ? node.mesh->name : "gltf_mesh");
				MeshPaths.push_back(path);
				SetMeshTransform(static_cast<int>(Meshes.size()) - 1, transform);
				triTotal += triCount;
			}
		}

		cgltf_free(data);

		std::cout << "glTF caricato: " << path << " -> " << Meshes.size() << " mesh, "
			<< triTotal << " triangoli, " << Materials.size() << " materiali, "
			<< TexturePaths.size() << " texture" << std::endl;

		BuildBVH();
		return true;
	}

}
