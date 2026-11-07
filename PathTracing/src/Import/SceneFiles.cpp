#include "Import/SceneFiles.h"
#include "Import/Importer.h"

#include <map>

namespace Lucerna {

	bool OpenFile(const std::string& path, Scene& scene, AssetLibrary& assets) {
		const Importer* importer = FindImporter(path);
		if (importer == nullptr) {
			std::cerr << "Open: nessun importer per " << path << std::endl;
			return false;
		}
		// Si legge tutto a parte: se il file non va, la scena corrente resta intatta
		AssetLibrary loaded;
		ImportedScene imported;
		if (!importer->Import(path, loaded, imported))
			return false;

		// Un OBJ senza MTL, o una primitive glTF senza materiale, non ha niente da mostrare:
		// riceve un materiale di default, come il grigio di Blender.
		ObjectId fallback;
		for (auto& instance : imported.Instances)
			if (instance.Data.Material.IsNil()) {
				if (fallback.IsNil())
					fallback = loaded.AddMaterial(CreateDefaultPrincipled(), "Default");
				instance.Data.Material = fallback;
			}

		// Il file da' la posa e il campo visivo; l'obiettivo (profondita' di campo) non c'e' nei
		// formati che leggiamo, e resta quello di prima (ADR 0011).
		SceneCamera camera = scene.Camera();
		if (imported.Camera) {
			camera.Position = imported.Camera->Position;
			camera.Direction = imported.Camera->Direction;
			camera.VerticalFOV = imported.Camera->VerticalFOV;
		}

		assets.Replace(std::move(loaded));
		scene.Clear();
		scene.SetCamera(camera);
		scene.SetSun(imported.Sun.value_or(SunParams{}));
		scene.SetBackground(imported.Background.value_or(glm::vec3(0.6f, 0.7f, 0.9f)), scene.EnvironmentMapping());
		for (const auto& light : imported.Lights)
			scene.AddLight(light.Data, light.Name);
		for (const auto& instance : imported.Instances)
			scene.AddMesh(instance.Data, instance.Name);
		return true;
	}

	int AddFile(const std::string& path, Scene& scene, AssetLibrary& assets, const glm::mat4& placement, const ObjectId& material) {
		const Importer* importer = FindImporter(path);
		if (importer == nullptr) {
			std::cerr << "Add: nessun importer per " << path << std::endl;
			return -1;
		}
		ImportedScene imported;
		if (!importer->Import(path, assets, imported))
			return -1;

		int first = -1;
		for (auto instance : imported.Instances) {
			instance.Data.Transform = placement * instance.Data.Transform;
			if (instance.Data.Material.IsNil())
				instance.Data.Material = material;
			const int index = scene.AddMesh(instance.Data, instance.Name);
			if (first < 0)
				first = index;
		}
		for (auto light : imported.Lights) {
			light.Data.Position = glm::vec3(placement * glm::vec4(light.Data.Position, 1.0f));
			light.Data.Direction = glm::normalize(glm::mat3(placement) * light.Data.Direction);
			scene.AddLight(light.Data, light.Name);
		}
		return first;
	}

	// Gli asset senza geometria, raggruppati per file: ogni file lo rilegge il suo importer una volta
	bool LoadMissingGeometry(AssetLibrary& assets) {
		std::map<std::string, std::vector<MeshAsset*>> byFile;
		for (MeshAsset* asset : assets.MeshesWithoutGeometry())
			byFile[asset->Source.File].push_back(asset);

		bool ok = true;
		for (const auto& [file, missing] : byFile) {
			const Importer* importer = FindImporter(file);
			if (importer == nullptr || !importer->LoadGeometry(file, missing)) {
				std::cerr << "Geometria non trovata: " << file << std::endl;
				ok = false;
			}
		}
		return ok;
	}
}
