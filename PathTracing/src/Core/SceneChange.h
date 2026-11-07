#pragma once

#ifndef CORE_SCENECHANGE_H
#define CORE_SCENECHANGE_H

#include <cstdint>

namespace Lucerna {

	// Cosa e' cambiato dall'ultimo frame (issue #25): il RenderScene aggiorna solo quello, e
	// qualunque cambiamento fa ripartire l'accumulo. La Scene e la AssetLibrary accendono
	// ciascuna i suoi; il Renderer li consuma insieme (issue #29).
	enum SceneChange : uint32_t {
		NoChange = 0,
		// ---- della Scene
		PrimitivesChanged = 1 << 0,  // sfere, quad, box: buffer, BLAS procedurale, light list
		MeshesChanged = 1 << 2,      // transform o materiale di un'istanza: MeshInfo e TLAS
		InstancesChanged = 1 << 3,   // istanze aggiunte o tolte: quali asset vanno sulla GPU
		LightsChanged = 1 << 4,      // luci punto e spot, Sole: light list
		EnvironmentChanged = 1 << 5, // sfondo, environment mapping
		CameraChanged = 1 << 7,      // l'obiettivo, o l'inquadratura spostata da fuori dalla vista
		// ---- della AssetLibrary
		MaterialsChanged = 1 << 1,   // buffer dei materiali e light list (l'emissione)
		TexturesChanged = 1 << 6,    // elenco delle texture
		GeometryChanged = 1 << 8,    // asset di geometria aggiunti o ricaricati: triangoli e BLAS

		SceneChanges = PrimitivesChanged | MeshesChanged | InstancesChanged | LightsChanged | EnvironmentChanged | CameraChanged,
		AssetChanges = MaterialsChanged | TexturesChanged | GeometryChanged,
		AllChanged = SceneChanges | AssetChanges,
	};
}

#endif // CORE_SCENECHANGE_H
