#pragma once

#include <string>

namespace PathTracer {

	class World;

	// Serializza/deserializza una scena su file JSON. Vive fuori da World e ne tocca solo
	// i membri pubblici + le API di costruzione (CreateBox, AddMesh, SetMeshTransform):
	// il modello dati non sa nulla della persistenza (pattern alla Cherno/Hazel).
	//
	// Nota: NON ri-carica i buffer GPU. Dopo un Deserialize riuscito il chiamante deve
	// forzare il re-upload del world (come per un cambio scena).
	class SceneSerializer {
	public:
		explicit SceneSerializer(World& world) : m_World(world) {}

		bool Serialize(const std::string& path) const;
		bool Deserialize(const std::string& path);

	private:
		World& m_World;
	};
}
