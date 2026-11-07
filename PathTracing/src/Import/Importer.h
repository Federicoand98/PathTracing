#pragma once

#ifndef IMPORT_IMPORTER_H
#define IMPORT_IMPORTER_H

#include "ptpch.h"
#include "Assets/AssetLibrary.h"
#include "Scene/Primitives.h"

#include <memory>
#include <optional>

namespace Lucerna {

	// Quello che un file porta oltre agli asset: le istanze delle sue mesh, le luci e, se le ha,
	// Sole, inquadratura e sfondo. Lo applicano OpenFile e AddFile (SceneFiles.h, issue #27).
	struct ImportedScene {
		struct Named { std::string Name; };
		struct Instance : Named { Mesh Data; };
		struct Light : Named { PunctualLight Data; };

		std::vector<Instance> Instances;   // Mesh::Asset cita un asset appena messo nella libreria
		std::vector<Light> Lights;
		std::optional<SunParams> Sun;
		std::optional<SceneCamera> Camera;
		std::optional<glm::vec3> Background;
	};

	/*
	* Un formato di file. Legge un file negli asset e in una ImportedScene, e sa rileggere la
	* geometria degli asset che vengono da un suo file (un file di scena salva solo la sorgente).
	* Non conosce la Scene: aprire o aggiungere e' affare di chi lo chiama.
	*/
	class Importer {
	public:
		virtual ~Importer() = default;

		virtual const char* Name() const = 0;                    // per il file dialog: "glTF 2.0"
		virtual std::vector<std::string> Extensions() const = 0; // minuscole, col punto: ".gltf"

		// false = file illeggibile, e allora la libreria resta com'era. La libreria deduplica:
		// una sorgente gia' caricata si riusa, e cosi' le texture.
		virtual bool Import(const std::string& path, AssetLibrary& assets, ImportedScene& scene) const = 0;

		// La geometria di questi asset, tutti con Source.File == file. false = qualcuno manca.
		virtual bool LoadGeometry(const std::string& file, const std::vector<MeshAsset*>& assets) const = 0;
	};

	// L'elenco dei formati: aggiungerne uno e' un importer nuovo e una riga in Importers().
	const std::vector<std::unique_ptr<Importer>>& Importers();
	const Importer* FindImporter(const std::string& path); // per estensione, senza badare alle maiuscole; nullptr = sconosciuta
}

#endif // IMPORT_IMPORTER_H
