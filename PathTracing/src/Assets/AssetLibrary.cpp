#include "Assets/AssetLibrary.h"

namespace Lucerna {

	const SceneObject<MeshAsset>* AssetLibrary::FindMesh(const MeshSource& source) const {
		for (const auto& mesh : m_Meshes)
			if (mesh.Source == source)
				return &mesh;
		return nullptr;
	}

	const SceneObject<MeshAsset>* AssetLibrary::FindMesh(const ObjectId& id) const {
		const int index = m_Meshes.IndexOf(id);
		return index >= 0 ? &m_Meshes[index] : nullptr;
	}

	bool AssetLibrary::IsTextureUsed(const ObjectId& id) const {
		for (const Material& m : m_Materials)
			if (m.AlbedoTexture == id || m.NormalTexture == id || m.AlphaTexture == id)
				return true;
		return false;
	}

	ObjectId AssetLibrary::AddMaterial(const Material& material, const std::string& name) {
		m_Changes |= MaterialsChanged;
		return m_Materials.Add(material, name).Id;
	}

	// i dati nuovi, l'identita' resta: SceneObject::operator= non tocca id e nome
	void AssetLibrary::Set(const ObjectId& id, const Material& material) {
		if (auto* object = m_Materials.Find(id)) {
			*object = material;
			m_Changes |= MaterialsChanged;
		}
	}

	ObjectId AssetLibrary::RegisterTexture(const std::string& path, TextureKind kind) {
		for (const auto& texture : m_Textures)
			if (texture.Path == path && texture.Kind == kind)
				return texture.Id;
		m_Changes |= TexturesChanged;
		return m_Textures.Add({ path, kind }, std::filesystem::path(path).stem().string()).Id;
	}

	bool AssetLibrary::RemoveTexture(const ObjectId& id) {
		const int index = m_Textures.IndexOf(id);
		if (index < 0 || IsTextureUsed(id))
			return false;
		m_Textures.erase(m_Textures.begin() + index);
		m_Changes |= TexturesChanged;
		return true;
	}

	ObjectId AssetLibrary::AddMesh(const MeshAsset& mesh, const std::string& name) {
		m_Changes |= GeometryChanged;
		return m_Meshes.Add(mesh, name).Id;
	}

	std::vector<MeshAsset*> AssetLibrary::MeshesWithoutGeometry() {
		std::vector<MeshAsset*> missing;
		for (auto& mesh : m_Meshes)
			if (mesh.Triangles.empty())
				missing.push_back(&mesh);
		if (!missing.empty())
			m_Changes |= GeometryChanged;
		return missing;
	}

	bool AssetLibrary::Rename(const ObjectId& id, const std::string& name) {
		auto rename = [&](auto& list) {
			if (auto* object = list.Find(id)) {
				list.Rename(*object, name);
				return true;
			}
			return false;
		};
		return rename(m_Materials) || rename(m_Textures) || rename(m_Meshes);
	}

	void AssetLibrary::Clear() {
		m_Materials.clear();
		m_Textures.clear();
		m_Meshes.clear();
		m_Changes |= AssetChanges;
	}

	void AssetLibrary::Replace(AssetLibrary&& other) {
		*this = std::move(other);
		m_Changes = AssetChanges;
	}
}
