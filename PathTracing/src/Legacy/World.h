#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "ptpch.h"

#include "Core/Ray.h"
#include "Assets/Material.h"
#include "Scene/Primitives.h"
#include "Import/Model.h"
#include "Scene/SceneObject.h"

struct cgltf_data; // GltfLoader.cpp: World.h non dipende da cgltf

namespace Lucerna {



	enum class SceneType {
		TWO_SPHERES = 0,
		RANDOM_SPHERES = 1,
		CORNELL_BOX = 2,
		RANDOM_BOXES = 3,
		CORNELL_BOX_MESH = 4,
		SETUP_1 = 5,
		SETUP_2 = 6,
		SETUP_3 = 7,
		SPONZA = 8
	};

	// Cosa e' cambiato nella scena dall'ultimo frame (issue #25): il RenderScene aggiorna solo
	// quello, e qualunque cambiamento fa ripartire l'accumulo.
	enum SceneChange : uint32_t {
		NoChange = 0,
		PrimitivesChanged = 1 << 0,  // sfere, quad, box: buffer, BLAS procedurale, light list
		MaterialsChanged = 1 << 1,   // buffer dei materiali e light list (l'emissione)
		MeshesChanged = 1 << 2,      // transform o materiale di una mesh: MeshInfo e TLAS
		GeometryChanged = 1 << 3,    // triangoli o elenco delle mesh: buffer dei triangoli e BLAS
		LightsChanged = 1 << 4,      // luci punto e spot, Sole: light list
		EnvironmentChanged = 1 << 5, // sfondo, environment mapping
		TexturesChanged = 1 << 6,    // elenco delle texture
		CameraChanged = 1 << 7,      // l'obiettivo o l'inquadratura spostata da fuori dalla vista
		AllChanged = 0xFF,
	};

	class World {
	public:
		World();
		~World();

		void LoadScene();
		void DestroyScene();

		// Importa una scena glTF (interop Blender, ADR 0003): rimpiazza la scena corrente con
		// geometria + materiali del file. Ritorna false se il parsing fallisce (scena invariata).
		// Definita in GltfLoader.cpp per non appesantire World.cpp con la dipendenza cgltf.
		bool LoadGltf(const std::string& path);

		// ---- lettura: le collezioni non si modificano da fuori (issue #25)
		const ObjectList<Sphere>& Spheres() const { return m_Spheres; }
		const ObjectList<Quad>& Quads() const { return m_Quads; }      // anche le 6 facce di ogni box
		const ObjectList<Box>& Boxes() const { return m_Boxes; }
		const ObjectList<Material>& Materials() const { return m_Materials; }
		const ObjectList<Mesh>& Meshes() const { return m_Meshes; }
		const ObjectList<PunctualLight>& PunctualLights() const { return m_PunctualLights; }
		const std::vector<Triangle>& Triangles() const { return m_Triangles; }
		const std::vector<std::string>& TexturePaths() const { return m_TexturePaths; }
		const std::vector<std::string>& DataTexturePaths() const { return m_DataTexturePaths; }
		const SunParams& Sun() const { return m_Sun; }
		const SceneCamera& Camera() const { return m_Camera; }
		const glm::vec3& BackgroundColor() const { return m_BackgroundColor; }
		bool EnvironmentMapping() const { return m_EnvironmentMapping; }

		// ---- modifica: si copia l'oggetto, si cambia la copia, la si riconsegna. Ogni
		// modifica registra cosa ha toccato; un id che non c'e' piu' non fa niente.
		void Set(const ObjectId& id, const Sphere& sphere);
		void Set(const ObjectId& id, const Quad& quad);
		void Set(const ObjectId& id, const Box& box);      // riscrive anche i suoi 6 quad
		void Set(const ObjectId& id, const Mesh& mesh);    // transform e materiale; i triangoli no
		void Set(const ObjectId& id, const Material& material);
		void Set(const ObjectId& id, const PunctualLight& light);
		void SetSun(const SunParams& sun);
		void SetCamera(const SceneCamera& camera);
		// La vista che naviga porta con se' l'inquadratura (ADR 0011): il renderer vede gia' il
		// movimento della vista e lo riproietta, quindi questo non e' un cambiamento della scena.
		void MoveCamera(const glm::vec3& position, const glm::vec3& direction);
		void SetBackground(const glm::vec3& color, bool environmentMapping);
		void Rename(const ObjectId& id, const std::string& name); // il nome non cambia l'immagine

		// Creazione a runtime. Sono tutte APPEND: gli indici gia' in giro (selezione,
		// MaterialIndex degli oggetti, Box::index) restano validi. Ritornano l'indice nuovo.
		int AddSphere(const Sphere& sphere);
		int AddQuad(const Quad& quad);
		int AddLight(const PunctualLight& light, const std::string& name);
		int AddMaterial(const Material& material);
		void AddTexture(const std::string& path);
		void CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex);
		int AddMesh(const std::string& objPath, const glm::vec3& position, int material);

		// Al contrario delle creazioni, le rimozioni NON sono append: invalidano ogni indice
		// successivo a quello tolto, nella collezione toccata. Chi tiene indici (la selezione)
		// deve azzerarli. Box::index viene rimappato qui dentro perche' nessun altro puo' farlo.
		bool RemoveSphere(int sphereIndex);
		bool RemoveQuad(int quadIndex);   // rifiuta i quad che appartengono a un box
		bool RemoveBox(int boxIndex);     // porta via anche i suoi 6 quad
		bool RemoveMesh(int meshIndex);
		bool RemoveLight(int lightIndex);

		// -1 se il quad e' autonomo, altrimenti l'indice del box che lo possiede
		int QuadOwnerBox(int quadIndex) const;

		// I cambiamenti accumulati dall'ultima chiamata; li consuma il renderer, una volta a
		// frame. Una scena nuova li ha tutti.
		uint32_t TakeChanges() { const uint32_t c = m_Changes; m_Changes = NoChange; return c; }

		int CurrentScene = 6; // quale scena carica LoadScene: scegliere non cambia niente finche' non si carica

	private:
		friend class SceneSerializer; // legge e rimpiazza la scena intera, poi segna tutto cambiato

		bool ImportLights(const cgltf_data* data); // KHR_lights_punctual: true se ha trovato luci
		void ImportCamera(const cgltf_data* data); // la prima camera prospettica, se c'e'

		uint32_t m_Changes = AllChanged;

		glm::vec3 m_BackgroundColor;
		SunParams m_Sun;                    // al massimo un Sole per scena (ADR 0006)
		bool m_EnvironmentMapping = false;  // true = cubemap, false = BackgroundColor
		SceneCamera m_Camera;               // l'inquadratura: la impostano le scene e i file (ADR 0011)
		// Ogni oggetto ha un id e un nome unico nella sua lista (issue #23): si aggiunge con Add.
		ObjectList<PunctualLight> m_PunctualLights{ "Light" }; // luci punto e spot (ADR 0009)
		std::vector<Test> m_Tests;
		ObjectList<Sphere> m_Spheres{ "Sphere" };
		ObjectList<Quad> m_Quads{ "Quad" };        // anche le 6 facce di ogni box, nascoste nell'outliner
		ObjectList<Box> m_Boxes{ "Box" };
		ObjectList<Material> m_Materials{ "Material" };
		ObjectList<Mesh> m_Meshes{ "Mesh" };
		std::vector<std::string> m_MeshPaths;       // path OBJ per la serializzazione
		// I triangoli di tutte le mesh, in local space e nell'ordine di caricamento: ogni Mesh ne
		// possiede un intervallo. I buffer GPU li deriva il RenderScene (issue #24).
		std::vector<Triangle> m_Triangles;
		std::vector<std::string> m_TexturePaths;     // un layer del sampler2DArray ciascuna
		std::vector<std::string> m_DataTexturePaths; // normal map e alpha mask MTL, in un array UNORM a parte

		void PrepareMaterials();
		void PrepareSimpleScene();
		void PrepareCornellBox();
		void PrepareCornellBoxMesh();
		void PrepareRandomSpheres();
		void PrepareRandomBoxes();
		void PrepareSetup1();
		void PrepareSetup2();
		void PrepareSetup3();
		void PrepareSponza();

		void UploadModel(const Model& model, const glm::vec3& Position, int material);

		// Traduzione MTL -> engine. Vivono qui perche' toccano Materials e TexturePaths.
		int RegisterMtlMaterials(const Model& model);      // appende i materiali MTL, ritorna l'indice base (-1 se l'OBJ non ha MTL)
		int RegisterTexture(const std::string& path);      // dedup in TexturePaths, ritorna il layer
		int RegisterDataTexture(const std::string& path); // idem in DataTexturePaths
		Material MaterialFromMtl(const MtlMaterial& mtl);  // MtlMaterial -> Material dell'engine

		void CreateCornellBox();
	};
}

#endif // WORLD_H