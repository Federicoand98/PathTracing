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
	// Formato 2 (issue #26): gli asset in un blocco a parte, gli oggetti li citano per id. Le mesh
	// salvano la sorgente (file, e per un glTF mesh e primitive), non i triangoli.

	static constexpr int FORMAT_VERSION = 2;

	bool SceneSerializer::Serialize(const std::string& path) const {
		const World& w = m_World;
		json root;
		root["version"] = FORMAT_VERSION;
		root["background"] = w.m_BackgroundColor;
		root["environmentMapping"] = w.m_EnvironmentMapping;
		root["camera"] = w.m_Camera;
		root["sun"] = w.m_Sun;
		if (m_Render)
			root["render"] = *m_Render;

		root["assets"]["textures"] = ObjectsToJson(w.m_Assets.Textures);
		root["assets"]["materials"] = ObjectsToJson(w.m_Assets.Materials);
		root["assets"]["meshes"] = ObjectsToJson(w.m_Assets.Meshes);

		root["punctualLights"] = ObjectsToJson(w.m_PunctualLights);
		root["spheres"] = ObjectsToJson(w.m_Spheres);
		root["boxes"] = ObjectsToJson(w.m_Boxes);
		root["meshes"] = ObjectsToJson(w.m_Meshes);
		// Solo i quad autonomi: quelli di un box li rigenera CreateBox al load
		root["quads"] = json::array();
		for (size_t i = 0; i < w.m_Quads.size(); i++) {
			if (w.QuadOwnerBox(static_cast<int>(i)) >= 0) continue;
			json j = static_cast<const Quad&>(w.m_Quads[i]);
			WriteIdentity(j, w.m_Quads[i].Id, w.m_Quads[i].Name);
			root["quads"].push_back(j);
		}

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

		// Il parse avviene PRIMA di toccare il world: un file malformato, o di un formato che non
		// si legge piu', lascia la scena corrente intatta invece che mezza sovrascritta.
		json root;
		try {
			file >> root;
		}
		catch (const std::exception& e) {
			std::cerr << "Deserialize: JSON non valido (" << e.what() << ")" << std::endl;
			return false;
		}
		if (root.value("version", 1) != FORMAT_VERSION) {
			std::cerr << "Deserialize: " << path << " e' nel formato " << root.value("version", 1)
			          << ", si legge solo il " << FORMAT_VERSION << std::endl;
			return false;
		}

		World& w = m_World;
		w.DestroyScene();

		// Un blocco assente lascia il valore com'e': il default del World appena svuotato
		root.value("background", json()).get_to(w.m_BackgroundColor);
		w.m_EnvironmentMapping = root.value("environmentMapping", w.m_EnvironmentMapping);
		root.value("sun", json()).get_to(w.m_Sun);
		root.value("camera", json()).get_to(w.m_Camera);
		if (m_Render)
			root.value("render", json()).get_to(*m_Render);

		const json assets = root.value("assets", json::object());
		ObjectsFromJson(assets.value("textures", json::array()), w.m_Assets.Textures);
		ObjectsFromJson(assets.value("materials", json::array()), w.m_Assets.Materials);
		ObjectsFromJson(assets.value("meshes", json::array()), w.m_Assets.Meshes);

		ObjectsFromJson(root.value("punctualLights", json::array()), w.m_PunctualLights);
		ObjectsFromJson(root.value("spheres", json::array()), w.m_Spheres);
		ObjectsFromJson(root.value("quads", json::array()), w.m_Quads);
		// I box vanno DOPO i quad autonomi: CreateBox appende i suoi 6 quad in coda e imposta
		// Box::index, cosi' l'invariante "index crescente" su cui conta lo shader regge.
		for (const json& bj : root.value("boxes", json::array())) {
			Box b;
			bj.get_to(b);
			w.CreateBox(glm::vec3(b.Min), glm::vec3(b.Max), b.Material);
			RestoreIdentity(w.m_Boxes, w.m_Boxes.back(), bj);
		}
		ObjectsFromJson(root.value("meshes", json::array()), w.m_Meshes);

		// la geometria si rilegge dalle sorgenti: un glTF si analizza una volta per tutte le sue mesh
		w.LoadMissingGeometry();

		std::cout << "Scena caricata: " << path << " (" << w.m_Spheres.size() << " sfere, "
			<< w.m_Boxes.size() << " box, " << w.m_Meshes.size() << " istanze di " << w.m_Assets.Meshes.size()
			<< " mesh, " << w.m_Assets.Materials.size() << " materiali)" << std::endl;
		w.m_Changes = AllChanged;
		return true;
	}
}
