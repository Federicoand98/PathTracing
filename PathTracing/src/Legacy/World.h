#pragma once

#ifndef WORLD_H
#define WORLD_H

#include "ptpch.h"

#include "Core/Ray.h"
#include "Assets/AssetLibrary.h"
#include "Scene/Primitives.h"
#include "Core/SceneObject.h"


namespace Lucerna {



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

		void DestroyScene();

		// I file passano dal registry degli importer (issue #27), quindi qui nessun formato.
		// Open rimpiazza la scena col contenuto del file: se l'importer fallisce, la scena resta
		// com'era. Un file senza camera tiene l'inquadratura corrente (ADR 0011).
		bool Open(const std::string& path);
		// Aggiunge le mesh e le luci del file alla scena: placement sposta tutto, material va ai
		// triangoli senza un materiale loro. Ritorna l'indice della prima istanza nuova, -1 se
		// il file non si legge o non ha mesh.
		int Add(const std::string& path, const glm::mat4& placement, const ObjectId& material);

		// Carica dalla sorgente la geometria degli asset che non ce l'hanno (letti da un file di
		// scena). false = qualcuna manca: le sue istanze non si vedono.
		bool LoadMissingGeometry();

		// ---- lettura: le collezioni non si modificano da fuori (issue #25)
		const ObjectList<Sphere>& Spheres() const { return m_Spheres; }
		const ObjectList<Quad>& Quads() const { return m_Quads; }      // anche le 6 facce di ogni box
		const ObjectList<Box>& Boxes() const { return m_Boxes; }
		const ObjectList<Mesh>& Meshes() const { return m_Meshes; }  // le istanze; la geometria e' negli asset
		const ObjectList<PunctualLight>& PunctualLights() const { return m_PunctualLights; }
		const AssetLibrary& Assets() const { return m_Assets; }
		const ObjectList<Material>& Materials() const { return m_Assets.Materials; }
		const ObjectList<Texture>& Textures() const { return m_Assets.Textures; }
		// L'asset di geometria di un'istanza (nullptr se manca) e la sua AABB world
		const MeshAsset* AssetOf(const Mesh& mesh) const;
		AABB WorldBounds(const Mesh& mesh) const;
		const SunParams& Sun() const { return m_Sun; }
		const SceneCamera& Camera() const { return m_Camera; }
		const glm::vec3& BackgroundColor() const { return m_BackgroundColor; }
		bool EnvironmentMapping() const { return m_EnvironmentMapping; }

		// ---- modifica: si copia l'oggetto, si cambia la copia, la si riconsegna. Ogni
		// modifica registra cosa ha toccato; un id che non c'e' piu' non fa niente.
		void Set(const ObjectId& id, const Sphere& sphere);
		void Set(const ObjectId& id, const Quad& quad);
		void Set(const ObjectId& id, const Box& box);      // riscrive anche i suoi 6 quad
		void Set(const ObjectId& id, const Mesh& mesh);    // transform e materiale; l'asset no
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
		void AddTexture(const std::string& path, TextureKind kind);
		void CreateBox(const glm::vec3& a, const glm::vec3& b, const ObjectId& material);
		int AddInstance(const ObjectId& asset, const glm::mat4& transform, const ObjectId& material);

		// Al contrario delle creazioni, le rimozioni NON sono append: invalidano ogni indice
		// successivo a quello tolto, nella collezione toccata. Chi tiene indici (la selezione)
		// deve azzerarli. Box::index viene rimappato qui dentro perche' nessun altro puo' farlo.
		bool RemoveSphere(int sphereIndex);
		bool RemoveQuad(int quadIndex);   // rifiuta i quad che appartengono a un box
		bool RemoveBox(int boxIndex);     // porta via anche i suoi 6 quad
		bool RemoveMesh(int meshIndex);
		bool RemoveLight(int lightIndex);
		bool RemoveTexture(const ObjectId& id); // solo se nessun materiale la usa: libera il suo layer

		// -1 se il quad e' autonomo, altrimenti l'indice del box che lo possiede
		int QuadOwnerBox(int quadIndex) const;

		// I cambiamenti accumulati dall'ultima chiamata; li consuma il renderer, una volta a
		// frame. Una scena nuova li ha tutti.
		uint32_t TakeChanges() { const uint32_t c = m_Changes; m_Changes = NoChange; return c; }


	private:
		friend class SceneSerializer; // legge e rimpiazza la scena intera, poi segna tutto cambiato


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
		ObjectList<Mesh> m_Meshes{ "Mesh" };
		AssetLibrary m_Assets;                     // geometria, materiali, texture (issue #26)

	};
}

#endif // WORLD_H