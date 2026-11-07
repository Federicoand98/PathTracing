#pragma once

#include <string>

namespace Lucerna {

	class Scene;
	class AssetLibrary;
	struct RenderSettings;

	// Serializza/deserializza scena e asset su file JSON. Vive fuori da Scene e AssetLibrary, che ne
	// sono friend:
	// il modello dati non sa nulla della persistenza (pattern alla Cherno/Hazel). Un Deserialize
	// riuscito rimpiazza la scena intera e la segna tutta cambiata, come un cambio scena.
	class SceneSerializer {
	public:
		// render: le impostazioni di render da salvare e ricaricare con la scena; nullptr = la
		// scena si salva senza, e al load non le tocca.
		SceneSerializer(Scene& scene, AssetLibrary& assets, RenderSettings* render = nullptr)
			: m_Scene(scene), m_Assets(assets), m_Render(render) {}

		bool Serialize(const std::string& path) const;
		bool Deserialize(const std::string& path);

	private:
		Scene& m_Scene;
		AssetLibrary& m_Assets;
		RenderSettings* m_Render;
	};
}
