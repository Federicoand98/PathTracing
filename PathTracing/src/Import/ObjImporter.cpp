// Importer OBJ/MTL (issue #27): il parser e' Model, qui l'OBJ diventa un asset coi materiali
// del suo MTL come slot, e un'istanza.

#include "Import/Importer.h"
#include "Import/Model.h"

namespace Lucerna {

	namespace {
		Material MaterialFromMtl(const MtlMaterial& mm, AssetLibrary& assets) {
			Material m;
			m.Color = glm::vec4(mm.Kd, 1.0f);

			// Ns (esponente Phong 0..1000) -> roughness percettiva. Pr, se presente, la sovrascrive.
			m.Roughness = glm::clamp(std::sqrt(2.0f / (mm.Ns + 2.0f)), 0.0f, 1.0f);
			if (mm.Pr >= 0.0f) m.Roughness = glm::clamp(mm.Pr, 0.0f, 1.0f);
			m.RefractionRoughness = m.Roughness;

			m.Metalness = (mm.Pm >= 0.0f) ? glm::clamp(mm.Pm, 0.0f, 1.0f) : 0.0f;
			m.RefractionRatio = glm::clamp(mm.Ni, 1.0f, 3.0f);
			m.SpecularProbability = 0.04f;       // F0 dielettrico standard
			m.SpecularColor = glm::vec4(1.0f);   // riflesso dielettrico non tinto (fisico)
			m.RefractionColor = m.Color;

			// emissione: la magnitudine di Ke diventa la forza, il resto e' il colore
			float ke = glm::max(mm.Ke.x, glm::max(mm.Ke.y, mm.Ke.z));
			if (ke > 0.0f) {
				m.EmissiveColor = glm::vec4(mm.Ke / ke, 1.0f);
				m.EmissiveStrenght = ke;
			}

			// map_Kd: la texture porta il colore, quindi Color va a bianco per non tingerla due volte
			if (!mm.map_Kd.empty()) {
				m.AlbedoTexture = assets.RegisterTexture(mm.map_Kd, TextureKind::Color);
				m.Color = glm::vec4(1.0f);
			}
			if (!mm.map_Bump.empty())
				m.NormalTexture = assets.RegisterTexture(mm.map_Bump, TextureKind::Data);
			// Molte copie di Sponza citano un _mask.tga che non distribuiscono e tengono la maschera
			// nell'alpha della diffusa: se il file manca si usa quella (AlphaTexture nil).
			if (!mm.map_d.empty()) {
				if (std::filesystem::exists(mm.map_d))
					m.AlphaTexture = assets.RegisterTexture(mm.map_d, TextureKind::Data);
				m.AlphaCutoff = 0.5f;
			}
			return m;
		}

		void CopyGeometry(const Model& model, MeshAsset& asset) {
			asset.Triangles.clear();
			asset.LocalMin = glm::vec4(1e30f);
			asset.LocalMax = glm::vec4(-1e30f);
			for (const Triangle* t : model.GetTriangles()) {
				asset.LocalMin = glm::min(asset.LocalMin, glm::min(t->A, glm::min(t->B, t->C)));
				asset.LocalMax = glm::max(asset.LocalMax, glm::max(t->A, glm::max(t->B, t->C)));
				asset.Triangles.push_back(*t);
			}
		}
	}

	class ObjImporter : public Importer {
	public:
		const char* Name() const override { return "Wavefront OBJ"; }
		std::vector<std::string> Extensions() const override { return { ".obj" }; }

		// Una sorgente gia' caricata si riusa (issue #26): due istanze dividono triangoli e BLAS,
		// e i materiali del MTL non si duplicano. Altrimenti i materiali del MTL diventano gli slot
		// dell'asset, e i triangoli citano lo slot col loro indice locale.
		bool Import(const std::string& path, AssetLibrary& assets, ImportedScene& scene) const override {
			Mesh instance;
			if (const auto* existing = assets.FindMesh({ path })) {
				instance.Asset = existing->Id;
				scene.Instances.push_back({ { existing->Name }, instance });
				return true;
			}

			Model model;
			model.LoadObj(path.c_str());
			if (model.GetTriangles().empty()) {
				std::cerr << "OBJ senza triangoli: " << path << std::endl;
				return false;
			}

			MeshAsset asset;
			asset.Source = { path };
			for (const MtlMaterial& mm : model.GetMaterials())
				asset.MaterialSlots.push_back(assets.Materials.Add(MaterialFromMtl(mm, assets), mm.Name).Id);
			CopyGeometry(model, asset);

			std::cout << "OBJ caricato: " << asset.Triangles.size() << " triangoli, "
				<< asset.MaterialSlots.size() << " materiali MTL" << std::endl;
			instance.Asset = assets.Meshes.Add(asset, model.GetName()).Id;
			scene.Instances.push_back({ { model.GetName() }, instance });
			return true;
		}

		bool LoadGeometry(const std::string& file, const std::vector<MeshAsset*>& assets) const override {
			Model model;
			model.LoadObj(file.c_str());
			for (MeshAsset* asset : assets)
				CopyGeometry(model, *asset);
			return !model.GetTriangles().empty();
		}
	};

	std::unique_ptr<Importer> MakeObjImporter() { return std::make_unique<ObjImporter>(); }
}
