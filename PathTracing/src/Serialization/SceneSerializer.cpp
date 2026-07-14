#include "SceneSerializer.h"

#include "World.h"

#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace PathTracer {

	// ---- glm <-> json ------------------------------------------------------

	static json ToJson(const glm::vec3& v) { return json::array({ v.x, v.y, v.z }); }
	static json ToJson(const glm::vec4& v) { return json::array({ v.x, v.y, v.z, v.w }); }
	static json ToJson(const glm::mat4& m) {
		const float* p = glm::value_ptr(m);
		json a = json::array();
		for (int i = 0; i < 16; i++) a.push_back(p[i]);
		return a;
	}

	static glm::vec3 ReadVec3(const json& j, const glm::vec3& d) {
		if (!j.is_array() || j.size() < 3) return d;
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
	}
	static glm::vec4 ReadVec4(const json& j, const glm::vec4& d) {
		if (!j.is_array() || j.size() < 4) return d;
		return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>() };
	}
	static glm::mat4 ReadMat4(const json& j, const glm::mat4& d) {
		if (!j.is_array() || j.size() < 16) return d;
		glm::mat4 m;
		float* p = glm::value_ptr(m);
		for (int i = 0; i < 16; i++) p[i] = j[i].get<float>();
		return m;
	}

	// ---- Material: campi con nome (leggibile e modificabile a mano) --------
	// Il padding std430 non si serializza. I campi assenti in un file restano al default
	// del costruttore, quindi un file vecchio resta caricabile quando aggiungo un campo.

	static json MaterialToJson(const Material& m) {
		return {
			{ "color", ToJson(m.Color) },
			{ "roughness", m.Roughness },
			{ "specularProbability", m.SpecularProbability },
			{ "refractionRatio", m.RefractionRatio },
			{ "emissiveStrength", m.EmissiveStrenght },
			{ "emissiveColor", ToJson(m.EmissiveColor) },
			{ "specularColor", ToJson(m.SpecularColor) },
			{ "refractionProbability", m.RefractionProbability },
			{ "refractionRoughness", m.RefractionRoughness },
			{ "refractionColor", ToJson(m.RefractionColor) },
			{ "albedoTexture", m.AlbedoTexture },
			{ "checker", m.Checker },
			{ "checkerScale", m.CheckerScale },
			{ "noiseType", m.NoiseType },
			{ "noiseScale", m.NoiseScale },
			{ "noiseBlend", m.NoiseBlend },
			{ "noiseOctaves", m.NoiseOctaves },
			{ "noiseTurbulence", m.NoiseTurbulence },
			{ "noiseColorA", ToJson(m.NoiseColorA) },
			{ "noiseColorB", ToJson(m.NoiseColorB) },
			{ "metalness", m.Metalness },
			{ "specularTint", m.SpecularTint },
		};
	}

	static Material MaterialFromJson(const json& j) {
		Material m; // parte dai default: i campi mancanti restano sensati
		auto f = [&](const char* k, float d) { return j.contains(k) ? j[k].get<float>() : d; };
		auto v4 = [&](const char* k, glm::vec4 d) { return j.contains(k) ? ReadVec4(j[k], d) : d; };

		m.Color = v4("color", m.Color);
		m.Roughness = f("roughness", m.Roughness);
		m.SpecularProbability = f("specularProbability", m.SpecularProbability);
		m.RefractionRatio = f("refractionRatio", m.RefractionRatio);
		m.EmissiveStrenght = f("emissiveStrength", m.EmissiveStrenght);
		m.EmissiveColor = v4("emissiveColor", m.EmissiveColor);
		m.SpecularColor = v4("specularColor", m.SpecularColor);
		m.RefractionProbability = f("refractionProbability", m.RefractionProbability);
		m.RefractionRoughness = f("refractionRoughness", m.RefractionRoughness);
		m.RefractionColor = v4("refractionColor", m.RefractionColor);
		m.AlbedoTexture = f("albedoTexture", m.AlbedoTexture);
		m.Checker = f("checker", m.Checker);
		m.CheckerScale = f("checkerScale", m.CheckerScale);
		m.NoiseType = f("noiseType", m.NoiseType);
		m.NoiseScale = f("noiseScale", m.NoiseScale);
		m.NoiseBlend = f("noiseBlend", m.NoiseBlend);
		m.NoiseOctaves = f("noiseOctaves", m.NoiseOctaves);
		m.NoiseTurbulence = f("noiseTurbulence", m.NoiseTurbulence);
		m.NoiseColorA = v4("noiseColorA", m.NoiseColorA);
		m.NoiseColorB = v4("noiseColorB", m.NoiseColorB);
		m.Metalness = f("metalness", m.Metalness);
		m.SpecularTint = f("specularTint", m.SpecularTint);
		return m;
	}

	// ---- Serialize ---------------------------------------------------------

	bool SceneSerializer::Serialize(const std::string& path) const {
		json root;
		root["version"] = 1;
		root["background"] = ToJson(m_World.BackgroundColor);

		for (const Material& m : m_World.Materials)
			root["materials"].push_back(MaterialToJson(m));

		for (const Sphere& s : m_World.Spheres)
			root["spheres"].push_back({ { "position", ToJson(s.Position) },
										{ "material", (int)s.MaterialIndex } });

		// Solo i quad autonomi: quelli di un box vengono rigenerati da CreateBox al load,
		// quindi salvarli li duplicherebbe.
		for (size_t i = 0; i < m_World.Quads.size(); i++) {
			if (m_World.QuadOwnerBox((int)i) >= 0) continue;
			const Quad& q = m_World.Quads[i];
			root["quads"].push_back({ { "posLLC", ToJson(q.PositionLLC) },
									  { "u", ToJson(q.U) }, { "v", ToJson(q.V) },
									  { "width", q.Width }, { "height", q.Height },
									  { "material", (int)q.MaterialIndex } });
		}

		// Un box e' definito da min/max/materiale: i 6 quad sono derivati.
		for (const Box& b : m_World.Boxes)
			root["boxes"].push_back({ { "min", ToJson(glm::vec3(b.Min)) },
									  { "max", ToJson(glm::vec3(b.Max)) },
									  { "material", (int)b.MaterialIndex } });

		// La mesh si salva come path OBJ + trasformazione + materiale: i triangoli e il BVH
		// si ricostruiscono al load. Una mesh senza path (sorgente ignota) viene saltata.
		for (size_t i = 0; i < m_World.Meshes.size(); i++) {
			const std::string meshPath = i < m_World.MeshPaths.size() ? m_World.MeshPaths[i] : "";
			if (meshPath.empty()) {
				std::cerr << "Serialize: mesh " << i << " senza path, saltata" << std::endl;
				continue;
			}
			root["meshes"].push_back({ { "path", meshPath },
									   { "transform", ToJson(m_World.Meshes[i].Transform) },
									   { "material", (int)m_World.Meshes[i].MaterialIndex } });
		}

		root["textures"] = m_World.TexturePaths;

		std::ofstream file(path);
		if (!file) {
			std::cerr << "Serialize: impossibile aprire " << path << std::endl;
			return false;
		}
		file << root.dump(2);
		std::cout << "Scena salvata: " << path << std::endl;
		return true;
	}

	// ---- Deserialize -------------------------------------------------------

	bool SceneSerializer::Deserialize(const std::string& path) {
		std::ifstream file(path);
		if (!file) {
			std::cerr << "Deserialize: file non trovato " << path << std::endl;
			return false;
		}

		// Il parse avviene PRIMA di toccare il world: un file malformato lancia e la scena
		// corrente resta intatta invece di finire mezza sovrascritta.
		json root;
		try {
			file >> root;
		}
		catch (const std::exception& e) {
			std::cerr << "Deserialize: JSON non valido (" << e.what() << ")" << std::endl;
			return false;
		}

		m_World.DestroyScene();

		m_World.BackgroundColor = ReadVec3(root.value("background", json()), glm::vec3(0.6f, 0.7f, 0.9f));

		for (const json& mj : root.value("materials", json::array()))
			m_World.Materials.push_back(MaterialFromJson(mj));

		for (const json& sj : root.value("spheres", json::array())) {
			Sphere s;
			s.Position = ReadVec4(sj.value("position", json()), glm::vec4(0.0f));
			s.MaterialIndex = (float)sj.value("material", 0);
			m_World.Spheres.push_back(s);
		}

		for (const json& qj : root.value("quads", json::array())) {
			Quad q;
			q.PositionLLC = ReadVec4(qj.value("posLLC", json()), glm::vec4(0.0f));
			q.U = ReadVec4(qj.value("u", json()), glm::vec4(0.0f));
			q.V = ReadVec4(qj.value("v", json()), glm::vec4(0.0f));
			q.Width = qj.value("width", 1.0f);
			q.Height = qj.value("height", 1.0f);
			q.MaterialIndex = (float)qj.value("material", 0);
			m_World.Quads.push_back(q);
		}

		// I box vanno DOPO i quad autonomi: CreateBox appende i suoi 6 quad in coda e imposta
		// Box::index, cosi' l'invariante "index crescente" su cui conta lo shader regge.
		for (const json& bj : root.value("boxes", json::array()))
			m_World.CreateBox(ReadVec3(bj.value("min", json()), glm::vec3(0.0f)),
							  ReadVec3(bj.value("max", json()), glm::vec3(1.0f)),
							  (float)bj.value("material", 0));

		m_World.TexturePaths = root.value("textures", std::vector<std::string>{});

		// AddMesh ricostruisce l'intero BVH a ogni chiamata: su tante mesh pesanti il load
		// puo' impiegare qualche secondo. Va bene, e' un'operazione one-shot.
		for (const json& mj : root.value("meshes", json::array())) {
			const std::string meshPath = mj.value("path", std::string());
			if (meshPath.empty()) continue;

			const int idx = m_World.AddMesh(meshPath, glm::vec3(0.0f), mj.value("material", 0));
			if (idx >= 0)
				m_World.SetMeshTransform(idx, ReadMat4(mj.value("transform", json()), glm::mat4(1.0f)));
		}

		std::cout << "Scena caricata: " << path << " ("
			<< m_World.Spheres.size() << " sfere, " << m_World.Boxes.size() << " box, "
			<< m_World.Meshes.size() << " mesh, " << m_World.Materials.size() << " materiali)" << std::endl;
		return true;
	}
}
