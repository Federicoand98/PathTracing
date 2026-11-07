#include "Legacy/World.h"
#include "Import/Importer.h"
#include <map>
#include "Assets/Material.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Lucerna {

	World::World() {
		m_BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);

        for(int i = 0; i < 10; i++) {
            m_Tests.push_back({glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)});
        }
	}

	World::~World() {
		m_Spheres.clear();
		m_Assets.Materials.clear();
	}

	void World::DestroyScene() {
		m_Spheres.clear();
		m_Quads.clear();
		m_Assets.Clear();
		m_Changes = AllChanged;
		m_Meshes.clear();
		m_Boxes.clear();
		m_Sun = SunParams{}; // spento: ogni scena accende il suo
		m_PunctualLights.clear();
		m_Camera = SceneCamera{}; // ogni scena imposta la sua inquadratura
	}

	// Un'istanza di asset. I triangoli restano in LOCAL SPACE: la posizione vive nella Transform.
	int World::AddInstance(const ObjectId& asset, const glm::mat4& transform, const ObjectId& material) {
		Mesh mesh;
		mesh.Asset = asset;
		mesh.Material = material;
		mesh.Transform = transform;
		const int index = m_Assets.Meshes.IndexOf(asset);
		m_Meshes.Add(mesh, index >= 0 ? m_Assets.Meshes[index].Name : std::string());
		m_Changes |= GeometryChanged | MeshesChanged;
		return static_cast<int>(m_Meshes.size()) - 1;
	}

	// ---- file (issue #27): ogni formato e' un importer del registry

	bool World::Open(const std::string& path) {
		const Importer* importer = FindImporter(path);
		if (importer == nullptr) {
			std::cerr << "Open: nessun importer per " << path << std::endl;
			return false;
		}
		// Si legge tutto a parte: se il file non va, la scena corrente resta intatta
		AssetLibrary assets;
		ImportedScene scene;
		if (!importer->Import(path, assets, scene))
			return false;

		const SceneCamera previousCamera = m_Camera;
		DestroyScene();
		m_Assets = std::move(assets);
		// Il file da' la posa e il campo visivo; l'obiettivo (profondita' di campo) non c'e' nei
		// formati che leggiamo, e resta quello di prima (ADR 0011).
		m_Camera = previousCamera;
		if (scene.Camera) {
			m_Camera.Position = scene.Camera->Position;
			m_Camera.Direction = scene.Camera->Direction;
			m_Camera.VerticalFOV = scene.Camera->VerticalFOV;
		}
		m_Sun = scene.Sun.value_or(SunParams{});
		m_BackgroundColor = scene.Background.value_or(glm::vec3(0.6f, 0.7f, 0.9f));
		for (const auto& light : scene.Lights)
			m_PunctualLights.Add(light.Data, light.Name);
		// Un OBJ senza MTL, o una primitive glTF senza materiale, non ha niente da mostrare:
		// riceve un materiale di default, come il grigio di Blender.
		ObjectId fallback;
		for (auto instance : scene.Instances) {
			if (instance.Data.Material.IsNil()) {
				if (fallback.IsNil())
					fallback = m_Assets.Materials.Add(CreateDefaultPrincipled(), "Default").Id;
				instance.Data.Material = fallback;
			}
			m_Meshes.Add(instance.Data, instance.Name);
		}
		m_Changes = AllChanged;
		return true;
	}

	// Sole, inquadratura e sfondo del file non si toccano: la scena e' quella corrente
	int World::Add(const std::string& path, const glm::mat4& placement, const ObjectId& material) {
		const Importer* importer = FindImporter(path);
		if (importer == nullptr) {
			std::cerr << "Add: nessun importer per " << path << std::endl;
			return -1;
		}
		ImportedScene scene;
		if (!importer->Import(path, m_Assets, scene))
			return -1;

		const int first = static_cast<int>(m_Meshes.size());
		for (auto instance : scene.Instances) {
			instance.Data.Transform = placement * instance.Data.Transform;
			if (instance.Data.Material.IsNil())
				instance.Data.Material = material;
			m_Meshes.Add(instance.Data, instance.Name);
		}
		for (auto light : scene.Lights) {
			light.Data.Position = glm::vec3(placement * glm::vec4(light.Data.Position, 1.0f));
			light.Data.Direction = glm::normalize(glm::mat3(placement) * light.Data.Direction);
			m_PunctualLights.Add(light.Data, light.Name);
		}
		m_Changes |= GeometryChanged | MeshesChanged | MaterialsChanged | TexturesChanged | LightsChanged;
		return static_cast<int>(m_Meshes.size()) > first ? first : -1;
	}

	// Gli asset senza geometria, raggruppati per file: ogni file lo rilegge il suo importer una volta
	bool World::LoadMissingGeometry() {
		std::map<std::string, std::vector<MeshAsset*>> byFile;
		for (auto& asset : m_Assets.Meshes)
			if (asset.Triangles.empty())
				byFile[asset.Source.File].push_back(&asset);

		bool ok = true;
		for (const auto& [file, assets] : byFile) {
			const Importer* importer = FindImporter(file);
			if (importer == nullptr || !importer->LoadGeometry(file, assets)) {
				std::cerr << "Geometria non trovata: " << file << std::endl;
				ok = false;
			}
		}
		m_Changes |= GeometryChanged | MeshesChanged;
		return ok;
	}

	int World::QuadOwnerBox(int quadIndex) const {
		for (size_t b = 0; b < m_Boxes.size(); b++) {
			const int first = static_cast<int>(m_Boxes[b].index);
			if (quadIndex >= first && quadIndex < first + 6)
				return static_cast<int>(b);
		}
		return -1;
	}

	// Nessuno tiene indici verso le sfere, a parte la selezione: qui basta la erase.
	bool World::RemoveSphere(int sphereIndex) {
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
	bool World::RemoveQuad(int quadIndex) {
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

	bool World::RemoveBox(int boxIndex) {
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
	bool World::RemoveMesh(int meshIndex) {
		if (meshIndex < 0 || meshIndex >= static_cast<int>(m_Meshes.size()))
			return false;
		m_Meshes.erase(m_Meshes.begin() + meshIndex);
		m_Changes |= GeometryChanged | MeshesChanged;
		return true;
	}

	void World::CreateBox(const glm::vec3& a, const glm::vec3& b, const ObjectId& material) {
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

	void World::Set(const ObjectId& id, const Sphere& sphere) {
		if (Assign(m_Spheres, id, sphere)) m_Changes |= PrimitivesChanged;
	}
	void World::Set(const ObjectId& id, const Quad& quad) {
		if (Assign(m_Quads, id, quad)) m_Changes |= PrimitivesChanged;
	}
	void World::Set(const ObjectId& id, const Box& box) {
		auto* object = m_Boxes.Find(id);
		if (object == nullptr)
			return;
		*object = box;
		object->UpdateBox(m_Quads); // un box e' 6 quad: vanno riscritti
		m_Changes |= PrimitivesChanged;
	}
	void World::Set(const ObjectId& id, const Mesh& mesh) {
		auto* object = m_Meshes.Find(id);
		if (object == nullptr)
			return;
		// l'asset e' la geometria: qui cambiano solo transform e materiale
		object->Transform = mesh.Transform;
		object->Material = mesh.Material;
		m_Changes |= MeshesChanged;
	}
	void World::Set(const ObjectId& id, const Material& material) {
		if (Assign(m_Assets.Materials, id, material)) m_Changes |= MaterialsChanged;
	}
	void World::Set(const ObjectId& id, const PunctualLight& light) {
		if (Assign(m_PunctualLights, id, light)) m_Changes |= LightsChanged;
	}
	void World::SetSun(const SunParams& sun) {
		m_Sun = sun;
		m_Changes |= LightsChanged;
	}
	void World::SetCamera(const SceneCamera& camera) {
		m_Camera = camera;
		m_Changes |= CameraChanged;
	}
	void World::MoveCamera(const glm::vec3& position, const glm::vec3& direction) {
		m_Camera.Position = position;
		m_Camera.Direction = direction;
	}
	void World::SetBackground(const glm::vec3& color, bool environmentMapping) {
		m_BackgroundColor = color;
		m_EnvironmentMapping = environmentMapping;
		m_Changes |= EnvironmentChanged;
	}

	// gli id sono unici in tutta la scena: basta cercarlo in ogni lista
	void World::Rename(const ObjectId& id, const std::string& name) {
		auto rename = [&](auto& list) {
			if (auto* object = list.Find(id)) {
				list.Rename(*object, name);
				return true;
			}
			return false;
		};
		rename(m_Spheres) || rename(m_Quads) || rename(m_Boxes) || rename(m_Meshes)
			|| rename(m_PunctualLights) || rename(m_Assets.Materials);
	}

	int World::AddSphere(const Sphere& sphere) {
		m_Spheres.Add(sphere);
		m_Changes |= PrimitivesChanged;
		return static_cast<int>(m_Spheres.size()) - 1;
	}
	int World::AddQuad(const Quad& quad) {
		m_Quads.Add(quad);
		m_Changes |= PrimitivesChanged;
		return static_cast<int>(m_Quads.size()) - 1;
	}
	int World::AddLight(const PunctualLight& light, const std::string& name) {
		m_PunctualLights.Add(light, name);
		m_Changes |= LightsChanged;
		return static_cast<int>(m_PunctualLights.size()) - 1;
	}
	int World::AddMaterial(const Material& material) {
		m_Assets.Materials.Add(material);
		m_Changes |= MaterialsChanged;
		return static_cast<int>(m_Assets.Materials.size()) - 1;
	}
	void World::AddTexture(const std::string& path, TextureKind kind) {
		m_Assets.RegisterTexture(path, kind);
		m_Changes |= TexturesChanged;
	}

	bool World::RemoveTexture(const ObjectId& id) {
		const int index = m_Assets.Textures.IndexOf(id);
		if (index < 0 || m_Assets.IsTextureUsed(id))
			return false;
		m_Assets.Textures.erase(m_Assets.Textures.begin() + index);
		m_Changes |= TexturesChanged;
		return true;
	}

	const MeshAsset* World::AssetOf(const Mesh& mesh) const {
		for (const auto& asset : m_Assets.Meshes)
			if (asset.Id == mesh.Asset)
				return &asset;
		return nullptr;
	}

	AABB World::WorldBounds(const Mesh& mesh) const {
		AABB box;
		const MeshAsset* asset = AssetOf(mesh);
		if (asset == nullptr)
			return box;
		for (int corner = 0; corner < 8; corner++)
			box.expand(glm::vec3(mesh.Transform * glm::vec4(
				(corner & 1) ? asset->LocalMax.x : asset->LocalMin.x,
				(corner & 2) ? asset->LocalMax.y : asset->LocalMin.y,
				(corner & 4) ? asset->LocalMax.z : asset->LocalMin.z, 1.0f)));
		return box;
	}

	bool World::RemoveLight(int lightIndex) {
		if (lightIndex < 0 || lightIndex >= static_cast<int>(m_PunctualLights.size()))
			return false;
		m_PunctualLights.erase(m_PunctualLights.begin() + lightIndex);
		m_Changes |= LightsChanged;
		return true;
	}
}
