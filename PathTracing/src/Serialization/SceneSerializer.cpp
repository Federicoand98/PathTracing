#include "Serialization/SceneSerializer.h"

#include "Legacy/World.h"
#include "Core/Json.h"
#include "Render/RenderSettings.h"

#include <nlohmann/json.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Lucerna {

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

	// ---- identita' degli oggetti (issue #23) ---------------------------------
	// Ogni oggetto salvato porta "id" e "name" accanto ai suoi campi. Un file senza (salvato
	// prima dell'issue) carica gli oggetti con un id nuovo e il nome di default del tipo.

	static void WriteIdentity(json& j, const ObjectId& id, const std::string& name) {
		j["id"] = id.ToString();
		j["name"] = name;
	}

	static ObjectId ReadId(const json& j) {
		if (auto it = j.find("id"); it != j.end() && it->is_string())
			if (std::optional<ObjectId> id = ObjectId::Parse(it->get<std::string>()))
				return *id;
		return {};
	}

	static std::string ReadName(const json& j) { return j.value("name", std::string()); }

	// Le liste i cui oggetti hanno una tabella di campi (materiali, luci)
	template<class T>
	static json ObjectsToJson(const ObjectList<T>& list) {
		json a = json::array();
		for (const SceneObject<T>& o : list) {
			json j = static_cast<const T&>(o);
			WriteIdentity(j, o.Id, o.Name);
			a.push_back(j);
		}
		return a;
	}

	template<class T>
	static void ObjectsFromJson(const json& a, ObjectList<T>& list) {
		if (!a.is_array()) return;
		for (const json& j : a) {
			T data{};
			j.get_to(data);
			list.Add(data, ReadName(j), ReadId(j));
		}
	}

	// Un oggetto creato da una funzione del World (CreateBox, AddMesh) riceve dopo l'identita' salvata
	template<class T>
	static void RestoreIdentity(ObjectList<T>& list, SceneObject<T>& o, const json& j) {
		if (ObjectId id = ReadId(j); !id.IsNil()) o.Id = id;
		if (std::string name = ReadName(j); !name.empty()) list.Rename(o, name);
	}

	// ---- Serialize ---------------------------------------------------------

	bool SceneSerializer::Serialize(const std::string& path) const {
		json root;
		root["version"] = 1;
		root["background"] = ToJson(m_World.m_BackgroundColor);
		root["environmentMapping"] = m_World.m_EnvironmentMapping;
		root["camera"] = m_World.m_Camera;
		root["sun"] = m_World.m_Sun;
		root["punctualLights"] = ObjectsToJson(m_World.m_PunctualLights);
		root["materials"] = ObjectsToJson(m_World.m_Materials);
		if (m_Render)
			root["render"] = *m_Render;

		for (const SceneObject<Sphere>& s : m_World.m_Spheres) {
			json j = { { "position", ToJson(s.Position) }, { "material", (int)s.MaterialIndex } };
			WriteIdentity(j, s.Id, s.Name);
			root["spheres"].push_back(j);
		}

		// Solo i quad autonomi: quelli di un box vengono rigenerati da CreateBox al load,
		// quindi salvarli li duplicherebbe.
		for (size_t i = 0; i < m_World.m_Quads.size(); i++) {
			if (m_World.QuadOwnerBox((int)i) >= 0) continue;
			const SceneObject<Quad>& q = m_World.m_Quads[i];
			json j = { { "posLLC", ToJson(q.PositionLLC) }, { "u", ToJson(q.U) }, { "v", ToJson(q.V) },
			           { "width", q.Width }, { "height", q.Height }, { "material", (int)q.MaterialIndex } };
			WriteIdentity(j, q.Id, q.Name);
			root["quads"].push_back(j);
		}

		// Un box e' definito da min/max/materiale: i 6 quad sono derivati.
		for (const SceneObject<Box>& b : m_World.m_Boxes) {
			json j = { { "min", ToJson(glm::vec3(b.Min)) }, { "max", ToJson(glm::vec3(b.Max)) }, { "material", (int)b.MaterialIndex } };
			WriteIdentity(j, b.Id, b.Name);
			root["boxes"].push_back(j);
		}

		// La mesh si salva come path OBJ + trasformazione + materiale: i triangoli e i BLAS
		// si ricostruiscono al load. Una mesh senza path (sorgente ignota) viene saltata.
		for (size_t i = 0; i < m_World.m_Meshes.size(); i++) {
			const std::string meshPath = i < m_World.m_MeshPaths.size() ? m_World.m_MeshPaths[i] : "";
			if (meshPath.empty()) {
				std::cerr << "Serialize: mesh " << i << " senza path, saltata" << std::endl;
				continue;
			}
			json j = { { "path", meshPath }, { "transform", ToJson(m_World.m_Meshes[i].Transform) },
			           { "material", (int)m_World.m_Meshes[i].MaterialIndex } };
			WriteIdentity(j, m_World.m_Meshes[i].Id, m_World.m_Meshes[i].Name);
			root["meshes"].push_back(j);
		}

		root["textures"] = m_World.m_TexturePaths;
		root["dataTextures"] = m_World.m_DataTexturePaths;

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

		m_World.m_BackgroundColor = ReadVec3(root.value("background", json()), glm::vec3(0.6f, 0.7f, 0.9f));

		m_World.m_EnvironmentMapping = root.value("environmentMapping", m_World.m_EnvironmentMapping);
		// Un blocco assente lascia il valore com'e': una scena salvata prima del Sole lo tiene
		// spento, come dopo DestroyScene, e una salvata prima di RenderSettings non tocca la
		// qualita' corrente.
		root.value("sun", json()).get_to(m_World.m_Sun);
		root.value("camera", json()).get_to(m_World.m_Camera);
		ObjectsFromJson(root.value("punctualLights", json::array()), m_World.m_PunctualLights);
		ObjectsFromJson(root.value("materials", json::array()), m_World.m_Materials);
		if (m_Render)
			root.value("render", json()).get_to(*m_Render);

		for (const json& sj : root.value("spheres", json::array())) {
			Sphere s;
			s.Position = ReadVec4(sj.value("position", json()), glm::vec4(0.0f));
			s.MaterialIndex = (float)sj.value("material", 0);
			m_World.m_Spheres.Add(s, ReadName(sj), ReadId(sj));
		}

		for (const json& qj : root.value("quads", json::array())) {
			Quad q;
			q.PositionLLC = ReadVec4(qj.value("posLLC", json()), glm::vec4(0.0f));
			q.U = ReadVec4(qj.value("u", json()), glm::vec4(0.0f));
			q.V = ReadVec4(qj.value("v", json()), glm::vec4(0.0f));
			q.Width = qj.value("width", 1.0f);
			q.Height = qj.value("height", 1.0f);
			q.MaterialIndex = (float)qj.value("material", 0);
			m_World.m_Quads.Add(q, ReadName(qj), ReadId(qj));
		}

		// I box vanno DOPO i quad autonomi: CreateBox appende i suoi 6 quad in coda e imposta
		// Box::index, cosi' l'invariante "index crescente" su cui conta lo shader regge.
		for (const json& bj : root.value("boxes", json::array())) {
			m_World.CreateBox(ReadVec3(bj.value("min", json()), glm::vec3(0.0f)),
							  ReadVec3(bj.value("max", json()), glm::vec3(1.0f)),
							  (float)bj.value("material", 0));
			RestoreIdentity(m_World.m_Boxes, m_World.m_Boxes.back(), bj);
		}

		m_World.m_TexturePaths = root.value("textures", std::vector<std::string>{});
		m_World.m_DataTexturePaths = root.value("dataTextures", std::vector<std::string>{});

		// AddMesh rifa' i buffer dei triangoli a ogni chiamata: su tante mesh pesanti il load
		// puo' impiegare qualche secondo. Va bene, e' un'operazione one-shot.
		for (const json& mj : root.value("meshes", json::array())) {
			const std::string meshPath = mj.value("path", std::string());
			if (meshPath.empty()) continue;

			const int idx = m_World.AddMesh(meshPath, glm::vec3(0.0f), mj.value("material", 0));
			if (idx >= 0) {
				m_World.m_Meshes.at(idx).Transform = ReadMat4(mj.value("transform", json()), glm::mat4(1.0f));
				RestoreIdentity(m_World.m_Meshes, m_World.m_Meshes[idx], mj);
			}
		}

		std::cout << "Scena caricata: " << path << " ("
			<< m_World.m_Spheres.size() << " sfere, " << m_World.m_Boxes.size() << " box, "
			<< m_World.m_Meshes.size() << " mesh, " << m_World.m_Materials.size() << " materiali)" << std::endl;
		m_World.m_Changes = AllChanged;
		return true;
	}
}
