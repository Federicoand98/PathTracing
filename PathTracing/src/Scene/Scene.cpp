#include "Scene/Scene.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Lucerna {

	void Scene::Clear() {
		m_Spheres.clear();
		m_Quads.clear();
		m_Changes = SceneChanges;
		m_Meshes.clear();
		m_Boxes.clear();
		m_Sun = SunParams{}; // spento: ogni scena accende il suo
		m_PunctualLights.clear();
		m_Camera = SceneCamera{}; // ogni scena imposta la sua inquadratura
	}

	// Un'istanza di asset. I triangoli restano in LOCAL SPACE: la posizione vive nella Transform.
	int Scene::AddMesh(const Mesh& mesh, const std::string& name) {
		m_Meshes.Add(mesh, name);
		m_Changes |= InstancesChanged | MeshesChanged;
		return static_cast<int>(m_Meshes.size()) - 1;
	}

	int Scene::QuadOwnerBox(int quadIndex) const {
		for (size_t b = 0; b < m_Boxes.size(); b++) {
			const int first = static_cast<int>(m_Boxes[b].index);
			if (quadIndex >= first && quadIndex < first + 6)
				return static_cast<int>(b);
		}
		return -1;
	}

	// Nessuno tiene indici verso le sfere, a parte la selezione: qui basta la erase.
	bool Scene::RemoveSphere(int sphereIndex) {
		if (sphereIndex < 0 || sphereIndex >= static_cast<int>(m_Spheres.size()))
			return false;

		m_Spheres.erase(m_Spheres.begin() + sphereIndex);
		m_Changes |= PrimitivesChanged;
		return true;
	}

	// I Box puntano ai quad per indice (Box::index = primo dei suoi 6, consecutivi).
	// Togliere un quad davanti a un box gli sposta i piedi sotto: va rimappato.
	// Lo shader scorre Quads e Cubes in lockstep e pretende FirstQuad crescente,
	// quindi la rimappatura per sottrazione conserva anche l'ordinamento.
	bool Scene::RemoveQuad(int quadIndex) {
		if (quadIndex < 0 || quadIndex >= static_cast<int>(m_Quads.size()))
			return false;

		// un quad di un box non e' un oggetto autonomo: si cancella il box, non la faccia
		if (QuadOwnerBox(quadIndex) >= 0)
			return false;

		m_Quads.erase(m_Quads.begin() + quadIndex);

		for (Box& box : m_Boxes)
			if (static_cast<int>(box.index) > quadIndex)
				box.index -= 1.0f;

		m_Changes |= PrimitivesChanged;
		return true;
	}

	bool Scene::RemoveBox(int boxIndex) {
		if (boxIndex < 0 || boxIndex >= static_cast<int>(m_Boxes.size()))
			return false;

		const int first = static_cast<int>(m_Boxes[boxIndex].index);
		if (first < 0 || first + 6 > static_cast<int>(m_Quads.size()))
			return false;

		m_Quads.erase(m_Quads.begin() + first, m_Quads.begin() + first + 6);
		m_Boxes.erase(m_Boxes.begin() + boxIndex);

		for (Box& box : m_Boxes)
			if (static_cast<int>(box.index) > first)
				box.index -= 6.0f;

		m_Changes |= PrimitivesChanged;
		return true;
	}

	// Toglie l'istanza. La geometria resta nella libreria: un'altra istanza puo' usarla, e un
	// asset senza istanze non costa niente sulla GPU (il RenderScene carica solo quelli usati).
	bool Scene::RemoveMesh(int meshIndex) {
		if (meshIndex < 0 || meshIndex >= static_cast<int>(m_Meshes.size()))
			return false;
		m_Meshes.erase(m_Meshes.begin() + meshIndex);
		m_Changes |= InstancesChanged | MeshesChanged;
		return true;
	}

	void Scene::CreateBox(const glm::vec3& a, const glm::vec3& b, const ObjectId& material) {
		Box box;
		box.Min = glm::vec4(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z), 0.0f);
		box.Max = glm::vec4(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z), 0.0f);

		glm::vec3 dx = glm::vec3(box.Max.x - box.Min.x, 0, 0);
		glm::vec3 dy = glm::vec3(0, box.Max.y - box.Min.y, 0);
		glm::vec3 dz = glm::vec3(0, 0, box.Max.z - box.Min.z);

		glm::vec3 nx = glm::normalize(dx);
		glm::vec3 ny = glm::normalize(dy);
		glm::vec3 nz = glm::normalize(dz);

		box.index = m_Quads.size();
		box.Material = material;

		Quad quad;

		quad.Material = material;

		// front
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Max.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			m_Quads.Add(quad, "Box face");
		}

		// right
		{
			quad.PositionLLC = { box.Max.x, box.Min.y, box.Max.z, 0 };
			quad.U = { -nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			m_Quads.Add(quad, "Box face");
		}

		// back
		{
			quad.PositionLLC = { box.Max.x, box.Min.y, box.Min.z, 0 };
			quad.U = { -nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			m_Quads.Add(quad, "Box face");
		}

		// left
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Min.z, 0 };
			quad.U = { nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			m_Quads.Add(quad, "Box face");
		}

		// top
		{
			quad.PositionLLC = { box.Min.x, box.Max.y, box.Max.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { -nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			m_Quads.Add(quad, "Box face");
		}

		// bottom
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Min.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			m_Quads.Add(quad, "Box face");
		}

		m_Boxes.Add(box);
		m_Changes |= PrimitivesChanged;
	}

	// ---- modifica (issue #25): ogni Set registra cosa ha toccato, il RenderScene aggiorna solo quello

	namespace {
		// i dati nuovi, l'identita' resta: SceneObject::operator= non tocca id e nome
		template<class T>
		bool Assign(ObjectList<T>& list, const ObjectId& id, const T& data) {
			auto* object = list.Find(id);
			if (object == nullptr)
				return false;
			*object = data;
			return true;
		}
	}

	void Scene::Set(const ObjectId& id, const Sphere& sphere) {
		if (Assign(m_Spheres, id, sphere)) m_Changes |= PrimitivesChanged;
	}
	void Scene::Set(const ObjectId& id, const Quad& quad) {
		if (Assign(m_Quads, id, quad)) m_Changes |= PrimitivesChanged;
	}
	void Scene::Set(const ObjectId& id, const Box& box) {
		auto* object = m_Boxes.Find(id);
		if (object == nullptr)
			return;
		*object = box;
		object->UpdateBox(m_Quads); // un box e' 6 quad: vanno riscritti
		m_Changes |= PrimitivesChanged;
	}
	void Scene::Set(const ObjectId& id, const Mesh& mesh) {
		auto* object = m_Meshes.Find(id);
		if (object == nullptr)
			return;
		// l'asset e' la geometria: qui cambiano solo transform e materiale
		object->Transform = mesh.Transform;
		object->Material = mesh.Material;
		m_Changes |= MeshesChanged;
	}
	void Scene::Set(const ObjectId& id, const PunctualLight& light) {
		if (Assign(m_PunctualLights, id, light)) m_Changes |= LightsChanged;
	}
	void Scene::SetSun(const SunParams& sun) {
		m_Sun = sun;
		m_Changes |= LightsChanged;
	}
	void Scene::SetCamera(const SceneCamera& camera) {
		m_Camera = camera;
		m_Changes |= CameraChanged;
	}
	void Scene::MoveCamera(const glm::vec3& position, const glm::vec3& direction) {
		m_Camera.Position = position;
		m_Camera.Direction = direction;
	}
	void Scene::SetBackground(const glm::vec3& color, bool environmentMapping) {
		m_BackgroundColor = color;
		m_EnvironmentMapping = environmentMapping;
		m_Changes |= EnvironmentChanged;
	}

	// gli id sono unici in tutta la scena: basta cercarlo in ogni lista
	bool Scene::Rename(const ObjectId& id, const std::string& name) {
		auto rename = [&](auto& list) {
			if (auto* object = list.Find(id)) {
				list.Rename(*object, name);
				return true;
			}
			return false;
		};
		return rename(m_Spheres) || rename(m_Quads) || rename(m_Boxes) || rename(m_Meshes) || rename(m_PunctualLights);
	}

	int Scene::AddSphere(const Sphere& sphere) {
		m_Spheres.Add(sphere);
		m_Changes |= PrimitivesChanged;
		return static_cast<int>(m_Spheres.size()) - 1;
	}
	int Scene::AddQuad(const Quad& quad) {
		m_Quads.Add(quad);
		m_Changes |= PrimitivesChanged;
		return static_cast<int>(m_Quads.size()) - 1;
	}
	int Scene::AddLight(const PunctualLight& light, const std::string& name) {
		m_PunctualLights.Add(light, name);
		m_Changes |= LightsChanged;
		return static_cast<int>(m_PunctualLights.size()) - 1;
	}
	bool Scene::RemoveLight(int lightIndex) {
		if (lightIndex < 0 || lightIndex >= static_cast<int>(m_PunctualLights.size()))
			return false;
		m_PunctualLights.erase(m_PunctualLights.begin() + lightIndex);
		m_Changes |= LightsChanged;
		return true;
	}

	AABB WorldBounds(const Mesh& mesh, const AssetLibrary& assets) {
		AABB box;
		const MeshAsset* asset = assets.FindMesh(mesh.Asset);
		if (asset == nullptr)
			return box;
		for (int corner = 0; corner < 8; corner++)
			box.expand(glm::vec3(mesh.Transform * glm::vec4(
				(corner & 1) ? asset->LocalMax.x : asset->LocalMin.x,
				(corner & 2) ? asset->LocalMax.y : asset->LocalMin.y,
				(corner & 4) ? asset->LocalMax.z : asset->LocalMin.z, 1.0f)));
		return box;
	}
}
