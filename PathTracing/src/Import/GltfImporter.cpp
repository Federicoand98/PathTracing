// Importer glTF 2.0 (interop Blender, ADR 0003; registry, issue #27). Il mapping rispecchia il
// percorso OBJ: un asset per primitive (triangoli in LOCAL space), un'istanza per nodo con la
// transform appiattita, e pbrMetallicRoughness -> Material come MaterialFromMtl fa con l'MTL.

#include "Import/Importer.h"

#include <filesystem>
#include <iostream>
#include <unordered_map>

#include "cgltf.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Lucerna {

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
		// della libreria) la fa il chiamante: qui il path della base color texture esce da baseTexOut.
		Material MaterialFromGltf(const cgltf_material& gm, const std::string& gltfDir, std::string& baseTexOut, std::string& normalTexOut) {
			Material m;
			baseTexOut.clear();
			normalTexOut.clear();

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

			// MASK e BLEND diventano entrambi una maschera sull'alpha dell'albedo: la trasparenza
			// parziale non e' gestita (ADR 0008). cgltf mette gia' a 0.5 il cutoff assente.
			if (gm.alpha_mode == cgltf_alpha_mode_mask)
				m.AlphaCutoff = glm::max(gm.alpha_cutoff, 1e-3f);
			else if (gm.alpha_mode == cgltf_alpha_mode_blend)
				m.AlphaCutoff = 0.5f;

			// normalTexture: convenzione OpenGL (+Y), e lo scale glTF e' la nostra NormalStrength
			if (gm.normal_texture.texture && gm.normal_texture.texture->image) {
				normalTexOut = ResolveTextureUri(gltfDir, gm.normal_texture.texture->image);
				m.NormalStrength = gm.normal_texture.scale;
			}
			return m;
		}

		// I triangoli di una primitive, in local space, e la loro AABB. false = non e' una
		// primitive di triangoli con posizioni, o non ne ha nessuno.
		bool ReadPrimitive(const cgltf_primitive& prim, MeshAsset& out) {
			if (prim.type != cgltf_primitive_type_triangles)
				return false;

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
				return false;

			const size_t vertCount = posA->count;
			std::vector<glm::vec3> pos(vertCount), nrm(vertCount);
			std::vector<glm::vec2> uv(vertCount, glm::vec2(0.0f));
			for (size_t v = 0; v < vertCount; v++) {
				cgltf_accessor_read_float(posA, v, glm::value_ptr(pos[v]), 3);
				if (nrmA) cgltf_accessor_read_float(nrmA, v, glm::value_ptr(nrm[v]), 3);
				if (uvA)  cgltf_accessor_read_float(uvA, v, glm::value_ptr(uv[v]), 2);
				// glTF ha l'origine delle UV in alto a sinistra, ma TextureArray capovolge le
				// immagini per la convenzione OBJ (origine in basso): senza questo le texture
				// glTF escono specchiate in verticale, e i normal map col verde invertito
				uv[v].y = 1.0f - uv[v].y;
			}

			// indici: se assenti, la primitive e' non indicizzata (0,1,2,3,...)
			const size_t idxCount = prim.indices ? prim.indices->count : vertCount;
			auto idxAt = [&](size_t i) -> cgltf_size {
				return prim.indices ? cgltf_accessor_read_index(prim.indices, i) : i;
			};

			out.Triangles.clear();
			out.LocalMin = glm::vec4(1e30f);
			out.LocalMax = glm::vec4(-1e30f);
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
				tri.MaterialSlot = -1.0f; // una primitive = un materiale: quello dell'istanza

				out.LocalMin = glm::min(out.LocalMin, glm::min(tri.A, glm::min(tri.B, tri.C)));
				out.LocalMax = glm::max(out.LocalMax, glm::max(tri.A, glm::max(tri.B, tri.C)));
				out.Triangles.push_back(tri);
			}
			return !out.Triangles.empty();
		}

		cgltf_data* ParseGltf(const std::string& path) {
			cgltf_options options{};
			cgltf_data* data = nullptr;
			if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
				std::cerr << "LoadGltf: parse fallito: " << path << std::endl;
				return nullptr;
			}
			// carica i buffer binari (.bin / data URI) referenziati dagli accessor
			if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
				std::cerr << "LoadGltf: load buffers fallito: " << path << std::endl;
				cgltf_free(data);
				return nullptr;
			}
			return data;
		}

		// KHR_lights_punctual (ADR 0009). glTF da' le intensita' in unita' fotometriche, che
	// l'esportatore di Blender ricava moltiplicando i suoi watt per 683 lm/W: dividendo per 683
	// la luce torna nelle unita' radiometriche di Blender, le stesse del Sole (irradianza).
	// Il range si ignora: e' un'ottimizzazione da motore real-time, non fisica.
		bool ImportLights(const cgltf_data* data, ImportedScene& scene) {
		constexpr float LUMENS_PER_WATT = 683.0f;
		bool any = false;

		for (size_t n = 0; n < data->nodes_count; n++) {
			const cgltf_node& node = data->nodes[n];
			if (!node.light)
				continue;
			const cgltf_light& gl = *node.light;

			float world[16];
			cgltf_node_transform_world(&node, world);
			const glm::mat4 m = glm::make_mat4(world);
			// una luce glTF punta lungo il -Z del suo nodo
			const glm::vec3 forward = glm::normalize(-glm::vec3(m[2]));
			const glm::vec3 color = glm::make_vec3(gl.color);

			if (gl.type == cgltf_light_type_directional) {
				if (scene.Sun) {
					std::cout << "glTF: piu' di una luce direzionale, ignorata: "
						<< (gl.name ? gl.name : "") << std::endl;
					continue;
				}
				// Direction() del Sole punta verso la luce: il contrario di dove va la luce
				const glm::vec3 toSun = -forward;
				SunParams sun;
				sun.Enabled = true;
				sun.Elevation = glm::degrees(std::asin(glm::clamp(toSun.y, -1.0f, 1.0f)));
				sun.Azimuth = glm::degrees(std::atan2(toSun.x, toSun.z));
				if (sun.Azimuth < 0.0f)
					sun.Azimuth += 360.0f;
				sun.Color = color;
				sun.Irradiance = gl.intensity / LUMENS_PER_WATT;
				scene.Sun = sun;
				any = true;
			}
			else if (gl.type == cgltf_light_type_point || gl.type == cgltf_light_type_spot) {
				PunctualLight p;
				p.Spot = gl.type == cgltf_light_type_spot;
				p.Position = glm::vec3(m[3]);
				p.Direction = forward;
				p.Color = color;
				p.Intensity = gl.intensity / LUMENS_PER_WATT;
				if (p.Spot) {
					p.InnerAngle = glm::degrees(gl.spot_inner_cone_angle);
					p.OuterAngle = glm::degrees(gl.spot_outer_cone_angle);
				}
				scene.Lights.push_back({ { gl.name ? gl.name : (node.name ? node.name : (p.Spot ? "Spot light" : "Point light")) }, p });
				any = true;
			}
		}

		if (any)
			std::cout << "glTF: " << scene.Lights.size() << " luci punto/spot"
				<< (scene.Sun ? ", Sole dalla luce direzionale" : "") << std::endl;
		return any;
	}

	// La prima camera prospettica del file diventa l'inquadratura: Blender esporta quella attiva.
	// yfov e' il FOV verticale, la stessa grandezza nostra. L'obiettivo (profondita' di campo)
	// non e' in glTF e resta com'e'.
		void ImportCamera(const cgltf_data* data, ImportedScene& scene) {
		for (size_t n = 0; n < data->nodes_count; n++) {
			const cgltf_node& node = data->nodes[n];
			if (!node.camera || node.camera->type != cgltf_camera_type_perspective)
				continue;
			float world[16];
			cgltf_node_transform_world(&node, world);
			const glm::mat4 m = glm::make_mat4(world);
			SceneCamera camera;
			camera.Position = glm::vec3(m[3]);
			camera.Direction = glm::normalize(-glm::vec3(m[2])); // una camera glTF guarda lungo -Z
			camera.VerticalFOV = glm::degrees(node.camera->data.perspective.yfov);
			scene.Camera = camera;
			std::cout << "glTF: camera " << (node.camera->name ? node.camera->name : "") << std::endl;
			return;
		}
	}

	} // namespace

	class GltfImporter : public Importer {
	public:
		const char* Name() const override { return "glTF 2.0"; }
		std::vector<std::string> Extensions() const override { return { ".gltf", ".glb" }; }

		bool Import(const std::string& path, AssetLibrary& assets, ImportedScene& scene) const override {
			cgltf_data* data = ParseGltf(path);
			if (data == nullptr)
				return false;

			ImportCamera(data, scene);
			// I raggi che mancano la geometria ritornano lo sfondo come radianza. Senza luci nel
			// file e' la sola illuminazione, quindi un cielo azzurro; con le luci si usa il grigio
			// scuro del World di default di Blender, che altrimenti sovrasterebbe le lampade.
			scene.Background = ImportLights(data, scene) ? glm::vec3(0.05f) : glm::vec3(0.6f, 0.7f, 0.9f);

			const std::string gltfDir = std::filesystem::path(path).parent_path().string();

			// Materiali: uno per materiale glTF, con una mappa puntatore -> id per le primitive.
			std::unordered_map<const cgltf_material*, ObjectId> materialIds;
			for (size_t i = 0; i < data->materials_count; i++) {
				const cgltf_material& gm = data->materials[i];
				std::string baseTex, normalTex;
				Material m = MaterialFromGltf(gm, gltfDir, baseTex, normalTex);
				if (!normalTex.empty())
					m.NormalTexture = assets.RegisterTexture(normalTex, TextureKind::Data);
				if (!baseTex.empty()) {
					m.AlbedoTexture = assets.RegisterTexture(baseTex, TextureKind::Color);
					m.Color = glm::vec4(1.0f); // la texture porta il colore, non tingerla due volte
				}
				materialIds[&gm] = assets.AddMaterial(m, gm.name ? gm.name : "");
			}

			size_t triTotal = 0;

			// Nodi: si visita l'intera gerarchia. cgltf_node_transform_world appiattisce la catena di
			// transform dalla radice. Ogni primitive diventa un asset, caricato una volta sola anche
			// se piu' nodi citano la stessa mesh (issue #26), e ogni nodo ne fa un'istanza.
			for (size_t n = 0; n < data->nodes_count; n++) {
				const cgltf_node& node = data->nodes[n];
				if (!node.mesh)
					continue;

				float world[16];
				cgltf_node_transform_world(&node, world);
				const glm::mat4 transform = glm::make_mat4(world);
				const int meshIndex = static_cast<int>(node.mesh - data->meshes);
				const std::string name = node.mesh->name ? node.mesh->name : (node.name ? node.name : "");

				for (size_t p = 0; p < node.mesh->primitives_count; p++) {
					const cgltf_primitive& prim = node.mesh->primitives[p];
					const MeshSource source{ path, meshIndex, static_cast<int>(p) };

					ObjectId asset;
					if (const auto* existing = assets.FindMesh(source)) {
						asset = existing->Id;
					} else {
						MeshAsset geometry;
						geometry.Source = source;
						if (!ReadPrimitive(prim, geometry))
							continue;
						triTotal += geometry.Triangles.size();
						asset = assets.AddMesh(geometry, name);
					}

					const auto material = prim.material ? materialIds.find(prim.material) : materialIds.end();
					Mesh instance;
					instance.Transform = transform;
					instance.Asset = asset;
					instance.Material = material != materialIds.end() ? material->second : ObjectId{};
					scene.Instances.push_back({ { name }, instance });
				}
			}

			cgltf_free(data);

			std::cout << "glTF caricato: " << path << " -> " << scene.Instances.size() << " istanze, "
				<< triTotal << " triangoli nuovi, " << assets.Materials().size() << " materiali" << std::endl;
			return true;
		}

		// Un file analizzato una volta sola per tutte le sue primitive
		bool LoadGeometry(const std::string& file, const std::vector<MeshAsset*>& assets) const override {
			cgltf_data* data = ParseGltf(file);
			bool ok = data != nullptr;
			for (MeshAsset* asset : assets) {
				const MeshSource& source = asset->Source;
				const bool found = data && source.Mesh >= 0 && source.Mesh < static_cast<int>(data->meshes_count)
				    && source.Primitive >= 0 && source.Primitive < static_cast<int>(data->meshes[source.Mesh].primitives_count)
				    && ReadPrimitive(data->meshes[source.Mesh].primitives[source.Primitive], *asset);
				ok &= found;
			}
			if (data)
				cgltf_free(data);
			return ok;
		}
	};

	std::unique_ptr<Importer> MakeGltfImporter() { return std::make_unique<GltfImporter>(); }
}
