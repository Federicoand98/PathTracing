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
		bool ImportLights(const cgltf_data* data); // KHR_lights_punctual: true se ha trovato luci
		void ImportCamera(const cgltf_data* data); // la prima camera prospettica, se c'e'

		// Creazione a runtime. Sono tutte APPEND: gli indici gia' in giro (selezione,
		// MaterialIndex degli oggetti, Box::index) restano validi.
		void CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex);
		int AddMesh(const std::string& objPath, const glm::vec3& position, int material);

		// Al contrario delle creazioni, le rimozioni NON sono append: invalidano ogni indice
		// successivo a quello tolto, nella collezione toccata. Chi tiene indici (la selezione)
		// deve azzerarli. Box::index viene rimappato qui dentro perche' nessun altro puo' farlo.
		bool RemoveSphere(int sphereIndex);
		bool RemoveQuad(int quadIndex);   // rifiuta i quad che appartengono a un box
		bool RemoveBox(int boxIndex);     // porta via anche i suoi 6 quad
		bool RemoveMesh(int meshIndex);

		// -1 se il quad e' autonomo, altrimenti l'indice del box che lo possiede
		int QuadOwnerBox(int quadIndex) const;

	public:
		glm::vec3 BackgroundColor;
		SunParams Sun;                    // al massimo un Sole per scena (ADR 0006)
		bool EnvironmentMapping = false;  // true = cubemap, false = BackgroundColor
		SceneCamera Camera;               // l'inquadratura: la impostano le scene e i file (ADR 0011)
		// Ogni oggetto ha un id e un nome unico nella sua lista (issue #23): si aggiunge con Add.
		ObjectList<PunctualLight> PunctualLights{ "Light" }; // luci punto e spot (ADR 0009)
        std::vector<Test> Tests;
		ObjectList<Sphere> Spheres{ "Sphere" };
		ObjectList<Quad> Quads{ "Quad" };        // anche le 6 facce di ogni box, nascoste nell'outliner
		ObjectList<Box> Boxes{ "Box" };
		ObjectList<Material> Materials{ "Material" };
		ObjectList<Mesh> Meshes{ "Mesh" };
		std::vector<std::string> MeshPaths;       // solo CPU, path OBJ per la serializzazione
		// I triangoli di tutte le mesh, in local space e nell'ordine di caricamento: ogni Mesh ne
		// possiede un intervallo. I buffer GPU li deriva il RenderScene (issue #24).
		std::vector<Triangle> Triangles;
		std::vector<std::string> TexturePaths;     // un layer del sampler2DArray ciascuna
		std::vector<std::string> DataTexturePaths; // normal map e alpha mask MTL, in un array UNORM a parte
		// Cresce a ogni modifica dei triangoli o dell'elenco delle mesh: il RenderScene ricarica
		// triangoli e BLAS solo quando cambia. Lo sostituira' il change tracking (#25).
		int GeometryVersion = 0;
		int CurrentScene = 6;
	private:
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