#pragma once

#ifndef IMPORT_SCENEFILES_H
#define IMPORT_SCENEFILES_H

#include "Scene/Scene.h"

namespace Lucerna {

	// I file entrano nella scena da qui, attraverso il registry degli importer (issue #27, #29):
	// la Scene e la AssetLibrary non conoscono nessun formato.

	// Rimpiazza scena e asset col contenuto del file. Se l'importer fallisce resta tutto com'era.
	// Un file senza camera tiene l'inquadratura corrente, e l'obiettivo resta sempre (ADR 0011).
	bool OpenFile(const std::string& path, Scene& scene, AssetLibrary& assets);

	// Aggiunge le mesh e le luci del file: placement sposta tutto, material va ai triangoli
	// senza un materiale loro. Ritorna l'indice della prima istanza nuova, -1 se il file non si
	// legge o non ha mesh. Sole, inquadratura e sfondo della scena restano.
	int AddFile(const std::string& path, Scene& scene, AssetLibrary& assets, const glm::mat4& placement, const ObjectId& material);

	// La geometria degli asset che non ce l'hanno (letti da un file di scena), dalla loro
	// sorgente. false = qualcuna manca: le sue istanze non si vedono.
	bool LoadMissingGeometry(AssetLibrary& assets);
}

#endif // IMPORT_SCENEFILES_H
