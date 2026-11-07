#pragma once

#ifndef ASSETS_ASSETLIBRARY_H
#define ASSETS_ASSETLIBRARY_H

#include "ptpch.h"
#include "Assets/Material.h"
#include "Core/SceneObject.h"

namespace Lucerna {

	/*
	* Gli asset (issue #26): geometria delle mesh, materiali e texture, ognuno con un id. Gli
	* oggetti della scena li citano per id invece di contenerli, come oggetti e data-block in
	* Blender: lo stesso modello piazzato due volte si carica una volta e ha un solo BLAS.
	* La libreria non sa leggere nessun formato: lo fanno i loader, che le consegnano asset fatti.
	*/

	enum class TextureKind { Color, Data }; // Color = sRGB (albedo), Data = UNORM (normal map, alpha mask)

	struct Texture {
		std::string Path;
		TextureKind Kind = TextureKind::Color;
	};

	// Un triangolo in local space. MaterialSlot indicizza gli slot della sua mesh; -1 = il
	// materiale dell'istanza (mono-materiale). Per gli OBJ multi-materiale e' l'usemtl.
	struct Triangle {
		glm::vec4 A;
		glm::vec4 B;
		glm::vec4 C;
		glm::vec4 NormalA;
		glm::vec4 NormalB;
		glm::vec4 NormalC;
		glm::vec2 UVA{ 0.0f };
		glm::vec2 UVB{ 0.0f };
		glm::vec2 UVC{ 0.0f };
		float MaterialSlot = -1.0f;
	};

	// Da dove viene una geometria: un OBJ intero, o una primitive di una mesh glTF. E' anche la
	// chiave con cui la libreria riconosce un asset gia' caricato, e quello che si salva nel JSON.
	struct MeshSource {
		std::string File;
		int Mesh = -1;       // glTF: indice della mesh; -1 per un OBJ
		int Primitive = -1;  // glTF: indice della primitive nella mesh
		bool operator==(const MeshSource&) const = default;
	};

	struct MeshAsset {
		MeshSource Source;
		std::vector<Triangle> Triangles;
		glm::vec4 LocalMin{ 0.0f }; // AABB in local space
		glm::vec4 LocalMax{ 0.0f };
		std::vector<ObjectId> MaterialSlots; // i materiali che i triangoli citano per slot
	};

	// Le tabelle per il file di scena (ADR 0010). I triangoli non si salvano: si rileggono dalla
	// sorgente, che e' tutto cio' che serve per ritrovarli.
	constexpr auto JsonFields(const Texture*) {
		return std::make_tuple(Field("path", &Texture::Path),
			Custom<Texture>("kind", [](const Texture& t) { return std::string(t.Kind == TextureKind::Color ? "color" : "data"); },
			                        [](Texture& t, const std::string& k) { t.Kind = k == "data" ? TextureKind::Data : TextureKind::Color; }));
	}

	constexpr auto JsonFields(const MeshSource*) {
		return std::make_tuple(Field("file", &MeshSource::File), Field("mesh", &MeshSource::Mesh), Field("primitive", &MeshSource::Primitive));
	}

	constexpr auto JsonFields(const MeshAsset*) {
		return std::make_tuple(Field("source", &MeshAsset::Source), Field("materialSlots", &MeshAsset::MaterialSlots));
	}

	class AssetLibrary {
	public:
		ObjectList<Material> Materials{ "Material" };
		ObjectList<Texture> Textures{ "Texture" };
		ObjectList<MeshAsset> Meshes{ "Mesh" };

		// L'asset con quella sorgente, se e' gia' caricato
		const SceneObject<MeshAsset>* FindMesh(const MeshSource& source) const {
			for (const auto& mesh : Meshes)
				if (mesh.Source == source)
					return &mesh;
			return nullptr;
		}

		// Una texture per path e tipo: due materiali con la stessa immagine la condividono
		ObjectId RegisterTexture(const std::string& path, TextureKind kind) {
			for (const auto& texture : Textures)
				if (texture.Path == path && texture.Kind == kind)
					return texture.Id;
			return Textures.Add({ path, kind }, std::filesystem::path(path).stem().string()).Id;
		}

		// Usata = citata da un materiale: solo una texture inutilizzata si puo' togliere
		bool IsTextureUsed(const ObjectId& id) const {
			for (const Material& m : Materials)
				if (m.AlbedoTexture == id || m.NormalTexture == id || m.AlphaTexture == id)
					return true;
			return false;
		}

		void Clear() {
			Materials.clear();
			Textures.clear();
			Meshes.clear();
		}
	};
}

#endif // ASSETS_ASSETLIBRARY_H
