#pragma once

#ifndef SCENE_SCENE_H
#define SCENE_SCENE_H

#include "ptpch.h"
#include "Assets/AssetLibrary.h"
#include "Core/SceneChange.h"
#include "Core/SceneObject.h"
#include "Scene/Primitives.h"

namespace Lucerna {

	/*
	* La descrizione della scena (issue #29): gli oggetti, le luci, il Sole, l'inquadratura e
	* l'ambiente. Gli asset (geometria, materiali, texture) stanno nella AssetLibrary accanto,
	* e gli oggetti li citano per id (ADR 0015). Come la GPU li vede lo decide il RenderScene;
	* i file li leggono gli importer (Import/SceneFiles.h). Qui non c'e' nessun formato e niente GPU.
	*
	* Le collezioni si leggono con accesso const e si modificano solo con l'API qui sotto, che
	* registra cosa e' cambiato (issue #25): una modifica che dimentica il flag non si puo' scrivere.
	*/
	class Scene {
	public:
		// ---- lettura
		const ObjectList<Sphere>& Spheres() const { return m_Spheres; }
		const ObjectList<Quad>& Quads() const { return m_Quads; }      // anche le 6 facce di ogni box
		const ObjectList<Box>& Boxes() const { return m_Boxes; }
		const ObjectList<Mesh>& Meshes() const { return m_Meshes; }  // le istanze; la geometria e' negli asset
		const ObjectList<PunctualLight>& PunctualLights() const { return m_PunctualLights; }
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
		void Set(const ObjectId& id, const PunctualLight& light);
		void SetSun(const SunParams& sun);
		void SetCamera(const SceneCamera& camera);
		// La vista che naviga porta con se' l'inquadratura (ADR 0011): il renderer vede gia' il
		// movimento della vista e lo riproietta, quindi questo non e' un cambiamento della scena.
		void MoveCamera(const glm::vec3& position, const glm::vec3& direction);
		void SetBackground(const glm::vec3& color, bool environmentMapping);
		bool Rename(const ObjectId& id, const std::string& name); // false = non e' un oggetto della scena

		// Creazione a runtime. Sono tutte APPEND: gli indici gia' in giro (selezione, Box::index)
		// restano validi. Ritornano l'indice nuovo.
		int AddSphere(const Sphere& sphere);
		int AddQuad(const Quad& quad);
		int AddLight(const PunctualLight& light, const std::string& name);
		int AddMesh(const Mesh& mesh, const std::string& name);
		void CreateBox(const glm::vec3& a, const glm::vec3& b, const ObjectId& material);

		// Al contrario delle creazioni, le rimozioni NON sono append: invalidano ogni indice
		// successivo a quello tolto, nella collezione toccata. Chi tiene indici (la selezione)
		// deve azzerarli. Box::index viene rimappato qui dentro perche' nessun altro puo' farlo.
		bool RemoveSphere(int sphereIndex);
		bool RemoveQuad(int quadIndex);   // rifiuta i quad che appartengono a un box
		bool RemoveBox(int boxIndex);     // porta via anche i suoi 6 quad
		bool RemoveMesh(int meshIndex);   // l'asset resta nella libreria
		bool RemoveLight(int lightIndex);

		// -1 se il quad e' autonomo, altrimenti l'indice del box che lo possiede
		int QuadOwnerBox(int quadIndex) const;

		// Svuota la scena: oggetti, luci, Sole e inquadratura; sfondo ed environment mapping restano
		void Clear();

		// I cambiamenti accumulati dall'ultima chiamata; li consuma il renderer, una volta a
		// frame. Una scena nuova li ha tutti.
		uint32_t TakeChanges() { const uint32_t c = m_Changes; m_Changes = NoChange; return c; }

	private:
		friend class SceneSerializer; // legge e rimpiazza la scena intera, poi segna tutto cambiato

		uint32_t m_Changes = SceneChanges;

		glm::vec3 m_BackgroundColor{ 0.6f, 0.7f, 0.9f };
		SunParams m_Sun;                    // al massimo un Sole per scena (ADR 0006)
		bool m_EnvironmentMapping = false;  // true = cubemap, false = BackgroundColor
		SceneCamera m_Camera;               // l'inquadratura: la impostano le scene e i file (ADR 0011)
		// Ogni oggetto ha un id e un nome unico nella sua lista (issue #23)
		ObjectList<PunctualLight> m_PunctualLights{ "Light" }; // luci punto e spot (ADR 0009)
		ObjectList<Sphere> m_Spheres{ "Sphere" };
		ObjectList<Quad> m_Quads{ "Quad" };        // anche le 6 facce di ogni box, nascoste nell'outliner
		ObjectList<Box> m_Boxes{ "Box" };
		ObjectList<Mesh> m_Meshes{ "Mesh" };
	};

	// L'AABB world di un'istanza: quella locale del suo asset, trasformata (vuota se manca)
	AABB WorldBounds(const Mesh& mesh, const AssetLibrary& assets);
}

#endif // SCENE_SCENE_H
