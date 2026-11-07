#include "Legacy/World.h"
#include "Assets/Material.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

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

	void World::LoadScene() {
		SceneType type = static_cast<SceneType>(CurrentScene);

		switch (type) {
		case SceneType::TWO_SPHERES:
			PrepareMaterials();
			PrepareSimpleScene();
			break;
		case SceneType::RANDOM_SPHERES:
			PrepareRandomSpheres();
			break;
		case SceneType::CORNELL_BOX:
			PrepareMaterials();
			PrepareCornellBox();
			break;
		case SceneType::CORNELL_BOX_MESH:
			PrepareMaterials();
			PrepareCornellBoxMesh();
			break;
		case SceneType::RANDOM_BOXES:
			PrepareRandomBoxes();
			break;
		case SceneType::SETUP_1:
			PrepareSetup1();
			break;
		case SceneType::SETUP_2:
			PrepareSetup2();
			break;
		case SceneType::SETUP_3:
			PrepareSetup3();
			break;
		case SceneType::SPONZA:
			PrepareSponza();
			break;
		default:
			break;
		}

		m_Changes = AllChanged;
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

	void World::PrepareMaterials() {
		Material redMaterial, blueMaterial, metal, lightMaterial, greenMaterial, whiteMaterial, glassMaterial;
		redMaterial.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
		redMaterial.SpecularColor = redMaterial.Color;
		redMaterial.Roughness = 1.0f;
		blueMaterial.Color = { 0.5f, 0.5f, 1.0f, 1.0f };
		blueMaterial.SpecularColor = blueMaterial.Color;
		blueMaterial.Roughness = 1.0f;
		metal.Color = { 0.3294f, 0.7019f, 0.6118f, 1.0f };
		metal.SpecularColor = metal.Color;
		metal.Roughness = 0.0f;
		lightMaterial.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
		lightMaterial.SpecularColor = lightMaterial.Color;
		lightMaterial.Roughness = 1.0f;
		lightMaterial.EmissiveColor = lightMaterial.Color;
		lightMaterial.EmissiveStrenght = 1.0f;
		greenMaterial.Color = { 0.0f, 1.0f, 0.0f, 1.0f };
		greenMaterial.SpecularColor = greenMaterial.Color;
		greenMaterial.Roughness = 1.0f;
		whiteMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		whiteMaterial.SpecularColor = whiteMaterial.Color;
		whiteMaterial.Roughness = 1.0f;
		glassMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		glassMaterial.SpecularColor = glassMaterial.Color;
		glassMaterial.RefractionRatio = 1.7f;

		m_Assets.Materials.Add(redMaterial);
		m_Assets.Materials.Add(blueMaterial);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(lightMaterial);
		m_Assets.Materials.Add(whiteMaterial);
		m_Assets.Materials.Add(greenMaterial);
		m_Assets.Materials.Add(glassMaterial);
	}

	void World::PrepareSimpleScene() {
		m_BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
		{
			Sphere s;
			s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
			s.Material = MaterialAt(2);
			// Spheres.Add(s);
		}
		{
			Sphere s;
			s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
			s.Material = MaterialAt(3);
			// Spheres.Add(s);
		}
		{
			Sphere s;
			s.Position = { 0.0f, -101.0f, 0.0f, 100.0f };
			s.Material = MaterialAt(0);
			m_Spheres.Add(s);
		}

		// Sfera gigante usata come terreno: scacchiera bianco/nero in world space.
		// Una scacchiera sulle UV non andrebbe: la camera sta quasi sul polo della sfera,
		// dove la mappatura equirettangolare degenera in spicchi convergenti.
		{
			Material& ground = m_Assets.Materials.at(0); // in questa scena il materiale 0 e' solo suo
			ground.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
			ground.SpecularColor = ground.Color;
			ground.Roughness = 1.0f;
			ground.SpecularProbability = 0.0f; // terreno opaco: niente riflesso speculare
			ground.Checker = 2.0f;             // world-space
			ground.CheckerScale = 0.5f;        // celle da 2 unita'
		}

		//CreateBox({ 0,0,0 }, { 3,1,1 }, MaterialAt(1));

		Model m;
		m.LoadObj("models/guitar.obj");

		// l'albedo moltiplica material.Color: se il materiale e' colorato la texture
		// ne esce tinta, quindi lo si porta a bianco
		Material& guitarMaterial = m_Assets.Materials.at(5);
		guitarMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		guitarMaterial.SpecularColor = guitarMaterial.Color;
		guitarMaterial.AlbedoTexture = m_Assets.RegisterTexture("textures/Albedo/uv_grid.png", TextureKind::Color);

		UploadModel(m, { 0.0, 0.0, 0.0 }, MaterialAt(5));
	}

	void World::PrepareCornellBox() {
		m_BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
		{
			Sphere s;
			s.Position = { -0.9f, -1.2f, 1.5f, 0.8f };
			s.Material = MaterialAt(2);
			m_Spheres.Add(s);
		}
		{
			Sphere s;
			s.Position = { 0.0f, -1.6f, 2.3f, 0.4f };
			s.Material = MaterialAt(2);
			m_Spheres.Add(s);
		}
		{
			Sphere s;
			s.Position = { 0.9f, -1.0f, 1.1f, 1.0f };
			s.Material = MaterialAt(2);
			m_Spheres.Add(s);
		}

		CreateCornellBox();
	}

	void World::PrepareCornellBoxMesh() {
		m_BackgroundColor = { 0.54f, 0.73f, 0.95f };
		CreateCornellBox();

		Model queen, king, knight;
		queen.LoadObj("models/queen.obj");
		king.LoadObj("models/king.obj");
		knight.LoadObj("models/knight.obj");

		UploadModel(queen, { 0.7, -2.1, 2.0 }, MaterialAt(2));
		UploadModel(king, { -1.2, -2.0, 1.6 }, MaterialAt(5));
		UploadModel(knight, { 0.9, -2.0, 3.0 }, MaterialAt(1));
	}

	void World::PrepareRandomSpheres() {
		m_BackgroundColor = { 0.54f, 0.73f, 0.95f };

		Sphere base = { {0.0f, -1001.0f, 0.0f, 1000.0f} };
		Sphere sphere1 = { {0.0f, 0.0f, 0.0f, 1.0f} };
		Sphere sphere2 = { {-3.0f, 0.0f, 0.0f, 1.0f} };
		Sphere sphere3 = { {3.0f, 0.0f, 0.0f, 1.0f} };

		Material dielectric, metal, diffuse, baseMat;
		dielectric = CreateDefaultDielectric();
		metal = CreateDefaultMetal();
		diffuse = CreateDefaultDiffuse();
		baseMat.Color = { 0.5f, 0.5f, 0.5f, 1.0f };
		baseMat.Roughness = 0.8f;
		baseMat.SpecularColor = baseMat.Color;

		// L'albedo moltiplica material.Color: un materiale colorato tingerebbe la texture,
		// quindi il colore va portato a bianco.
		diffuse.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		diffuse.SpecularColor = diffuse.Color;
		diffuse.SpecularProbability = 0.0f; // niente riflesso: la texture si vedrebbe poco
		diffuse.AlbedoTexture = m_Assets.RegisterTexture("textures/earthmap.jpg", TextureKind::Color);

		m_Assets.Materials.Add(dielectric);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(diffuse);   // materiale 2: solo di sphere2, la sfera "terra"
		m_Assets.Materials.Add(baseMat);
		// i materiali ci sono solo ora: 0 dielettrico, 1 metallo, 2 terra, 3 base
		base.Material = MaterialAt(3);
		sphere1.Material = MaterialAt(0);
		sphere2.Material = MaterialAt(2);
		sphere3.Material = MaterialAt(1);
		m_Spheres.Add(sphere1);
		m_Spheres.Add(sphere2);
		m_Spheres.Add(sphere3);
		m_Spheres.Add(base);

		for (int a = -11; a < 11; a++) {
			for (int b = -11; b < 11; b++) {
				glm::vec3 center(a + 0.9 * Random::GetFloat(0, 1), -0.82f, b + 0.9 * Random::GetFloat(0, 1));

				if (glm::length(center - glm::vec3(3, 0.2, 0)) > 0.9) {
					Material mat;
					mat = CreateRandom(true);
					m_Assets.Materials.Add(mat);

					Sphere s = { {center, 0.2f} };
					s.Material = MaterialAt(m_Assets.Materials.size() - 1);

					m_Spheres.Add(s);
				}
			}
		}

		/*
		for (int i = 4; i < m_Spheres.size(); i++) {
			m_Spheres.at(i).Material = MaterialAt(Random::GetFloat(3, m_Assets.Materials.size() - 1));
		}
		*/
	}

	void World::PrepareRandomBoxes() {
		m_BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
		Material dielectric, metal, diffuse, lightMaterial, baseMat;
		dielectric = CreateDefaultDielectric();
		metal = CreateDefaultMetal();
		diffuse = CreateDefaultDiffuse();
		baseMat.Color = { 0.5f, 0.5f, 0.5f, 1.0f };
		baseMat.SpecularColor = { 0.5f, 0.5f, 0.5f, 1.0f };
		baseMat.RefractionColor = { 0.5f, 0.5f, 0.5f, 1.0f };
		baseMat.Roughness = 0.8f;

		lightMaterial.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
		lightMaterial.Roughness = 1.0f;
		lightMaterial.EmissiveColor = lightMaterial.Color;
		lightMaterial.EmissiveStrenght = 1.0f;

		m_Assets.Materials.Add(dielectric);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(diffuse);
		m_Assets.Materials.Add(lightMaterial);
		m_Assets.Materials.Add(baseMat);

		{
			Sphere s;
			s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
			s.Material = MaterialAt(1);
			m_Spheres.Add(s);
		}
		{
			Sphere s;
			s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
			s.Material = MaterialAt(3);
			m_Spheres.Add(s);
		}

		for (int a = 0; a < 10; a++) {
			Material mat;
			mat = CreateRandom(false);

			m_Assets.Materials.Add(mat);
		}

		for (int x = -5; x < 6; x++) {
			for (int z = -5; z < 6; z++) {
				float y = Random::GetFloat(-4, -3);

				CreateBox({ x, y, z }, { x + 1, y + 1, z + 1 }, MaterialAt(Random::GetInt(5, m_Assets.Materials.size() - 1)));
			}
		}
	}

	void World::PrepareSetup1() {
		m_BackgroundColor = { 0.0, 0.0, 0.0 };

		Material white_diffuse = CreateDefaultDiffuse({ 1.0, 1.0, 1.0, 1.0 });
		Material white_metal = CreateDefaultMetal({ 1.0, 1.0, 1.0, 1.0 });
		Material white_glossy = CreateDefaultGlossy({ 1.0, 1.0, 1.0, 1.0 }, 0.2, 0.2);
		Material metal = CreateDefaultMetal({ .75, .75, .75, 1 });
		Material light = CreateDefaultLight({ 1.0, 1.0, 1.0, 1.0 }, 8.0);
		Material red, blue, green;
		red = CreateDefaultDiffuse({ 1.0, 0.0, 0.0, 1.0 });
		blue = CreateDefaultDiffuse({ 0.0, 0.0, 1.0, 1.0 });
		green = CreateDefaultDiffuse({ 0.0, 1.0, 0.0, 1.0 });
		Material glass1 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.45f);
		Material glass2 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.01f);
		// Metalli ramati veri (Metalness = 1): nel modello PBR il riflesso e' tinto dalla
		// base color. Prima erano Glossy dielettrici che fingevano il rame con lo specular
		// colorato; il roughness scalato ridà il gradiente nitido->satinato che davano.
		Material metal1 = CreateDefaultMetal({ 0.8549, 0.5490, 0.4, 1.0 }); metal1.Roughness = 0.05f;
		Material metal2 = CreateDefaultMetal({ 0.8549, 0.5490, 0.4, 1.0 }); metal2.Roughness = 0.25f;
		Material metal3 = CreateDefaultMetal({ 0.8549, 0.5490, 0.4, 1.0 }); metal3.Roughness = 0.45f;

		m_Assets.Materials.Add(white_diffuse);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(red);
		m_Assets.Materials.Add(blue);
		m_Assets.Materials.Add(green);
		m_Assets.Materials.Add(white_metal);
		m_Assets.Materials.Add(white_glossy);
		m_Assets.Materials.Add(glass1);
		m_Assets.Materials.Add(glass2);
		m_Assets.Materials.Add(light);
		m_Assets.Materials.Add(metal1);
		m_Assets.Materials.Add(metal2);
		m_Assets.Materials.Add(metal3);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.Material = MaterialAt(0);

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.Material = MaterialAt(6);

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.Material = MaterialAt(5);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.Material = MaterialAt(2);

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.Material = MaterialAt(3);

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.Material = MaterialAt(4);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.Material = MaterialAt(10);

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.Material = MaterialAt(11);

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.Material = MaterialAt(12);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, right;
			left.Position = { -1.75f, -0.9f, -2.75f, 0.4f };
			left.Material = MaterialAt(8);

			right.Position = { 1.75f, -0.9f, -2.75f, 0.4f };
			right.Material = MaterialAt(7);

			m_Spheres.Add(left);
			m_Spheres.Add(right);
		}

		{
			Quad bot, back;
			bot.PositionLLC = { -10.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 20.0f;
			bot.Height = 12.5f;
			bot.Material = MaterialAt(0);

			back.PositionLLC = { -10.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 20.0f;
			back.Height = 8.0f;
			back.Material = MaterialAt(0);

			m_Quads.Add(bot);
			m_Quads.Add(back);
		}

		{
			Quad panelLight1, panelLight2;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.Material = MaterialAt(9);

			panelLight2.PositionLLC = { -9.5f, 4.5f, -0.1f, 0.0f };
			panelLight2.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight2.V = { 0.0f, 1.0f, -1.0f, 0.0f };
			panelLight2.Width = 10.0f;
			panelLight2.Height = 8.0f;
			panelLight2.Material = MaterialAt(9);

			m_Quads.Add(panelLight2);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, MaterialAt(1));
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, MaterialAt(1));

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, MaterialAt(1));
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, MaterialAt(1));
	}

	void World::PrepareSetup2() {
		m_BackgroundColor = { 0.0, 0.0, 0.0 };

		Material white_diffuse = CreateDefaultDiffuse({ 1.0, 1.0, 1.0, 1.0 });
		Material white_metal = CreateDefaultMetal({ 1.0, 1.0, 1.0, 1.0 });
		Material white_glossy = CreateDefaultGlossy({ 1.0, 1.0, 1.0, 1.0 }, 0.2, 0.2);
		Material metal = CreateDefaultMetal({ .75, .75, .75, 1 });
		Material light = CreateDefaultLight({ 1.0, 1.0, 1.0, 1.0 }, 1.0);
		Material lightR = CreateDefaultLight({ 1.0, 0.0, 0.0, 1.0 }, 15.0);
		Material lightG = CreateDefaultLight({ 0.0, 1.0, 0.0, 1.0 }, 15.0);
		Material lightB = CreateDefaultLight({ 0.0, 0.0, 1.0, 1.0 }, 15.0);
		Material red, blue, green;
		red = CreateDefaultDiffuse({ 1.0, 0.0, 0.0, 1.0 });
		blue = CreateDefaultDiffuse({ 0.0, 0.0, 1.0, 1.0 });
		green = CreateDefaultDiffuse({ 0.0, 1.0, 0.0, 1.0 });
		Material glass1 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.45f);
		Material glass2 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.01f);

		m_Assets.Materials.Add(white_diffuse);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(red);
		m_Assets.Materials.Add(blue);
		m_Assets.Materials.Add(green);
		m_Assets.Materials.Add(white_metal);
		m_Assets.Materials.Add(white_glossy);
		m_Assets.Materials.Add(glass1);
		m_Assets.Materials.Add(glass2);
		m_Assets.Materials.Add(light);
		m_Assets.Materials.Add(lightR);
		m_Assets.Materials.Add(lightG);
		m_Assets.Materials.Add(lightB);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.Material = MaterialAt(0);

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.Material = MaterialAt(6);

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.Material = MaterialAt(5);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.Material = MaterialAt(2);

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.Material = MaterialAt(3);

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.Material = MaterialAt(4);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		/*
		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.Material = MaterialAt(9);

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.Material = MaterialAt(9);

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.Material = MaterialAt(9);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}
		*/

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -3.0f, 0.5f };
			left.Material = MaterialAt(10);

			middle.Position = { 0.0f, 2.2f, -3.0f, 0.5f };
			middle.Material = MaterialAt(12);

			right.Position = { 3.5f, 2.2f, -3.0f, 0.5f };
			right.Material = MaterialAt(11);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, right;
			left.Position = { -1.75f, -0.9f, -2.75f, 0.4f };
			left.Material = MaterialAt(8);

			right.Position = { 1.75f, -0.9f, -2.75f, 0.4f };
			right.Material = MaterialAt(7);

			m_Spheres.Add(left);
			m_Spheres.Add(right);
		}


		{
			Quad bot, back;
			bot.PositionLLC = { -10.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 20.0f;
			bot.Height = 12.5f;
			bot.Material = MaterialAt(0);

			back.PositionLLC = { -10.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 20.0f;
			back.Height = 8.0f;
			back.Material = MaterialAt(0);

			m_Quads.Add(bot);
			m_Quads.Add(back);
		}

		{
			Quad panelLight1;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.Material = MaterialAt(9);

			m_Quads.Add(panelLight1);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, MaterialAt(1));
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, MaterialAt(1));

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, MaterialAt(1));
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, MaterialAt(1));
	}

	void World::PrepareSetup3() {
		m_BackgroundColor = { 0.0, 0.0, 0.0 };

		Material white_diffuse = CreateDefaultDiffuse({ 1.0, 1.0, 1.0, 1.0 });
		Material white_metal = CreateDefaultMetal({ 1.0, 1.0, 1.0, 1.0 });
		Material white_glossy = CreateDefaultGlossy({ 1.0, 1.0, 1.0, 1.0 }, 0.2, 0.2);
		Material metal = CreateDefaultMetal({ .75, .75, .75, 1 });
		Material light = CreateDefaultLight({ 1.0, 1.0, 1.0, 1.0 }, 8.0);
		Material red, blue, green;
		red = CreateDefaultDiffuse({ 1.0, 0.0, 0.0, 1.0 });
		blue = CreateDefaultDiffuse({ 0.0, 0.0, 1.0, 1.0 });
		green = CreateDefaultDiffuse({ 0.0, 1.0, 0.0, 1.0 });
		Material glass1 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.45f);
		Material glass2 = CreateDefaultGlass(glm::vec4{ 1.0 }, 1.01f);

		m_Assets.Materials.Add(white_diffuse);
		m_Assets.Materials.Add(metal);
		m_Assets.Materials.Add(red);
		m_Assets.Materials.Add(blue);
		m_Assets.Materials.Add(green);
		m_Assets.Materials.Add(white_metal);
		m_Assets.Materials.Add(white_glossy);
		m_Assets.Materials.Add(glass1);
		m_Assets.Materials.Add(glass2);
		m_Assets.Materials.Add(light);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.Material = MaterialAt(0);

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.Material = MaterialAt(6);

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.Material = MaterialAt(5);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.Material = MaterialAt(2);

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.Material = MaterialAt(3);

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.Material = MaterialAt(4);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.Material = MaterialAt(0);

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.Material = MaterialAt(0);

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.Material = MaterialAt(0);

			m_Spheres.Add(left);
			m_Spheres.Add(middle);
			m_Spheres.Add(right);
		}

		Model left, right;
		left.LoadObj("models/king.obj");
		right.LoadObj("models/queen.obj");

		UploadModel(left, { -1.75, -1.4, -2.75 }, MaterialAt(3));
		UploadModel(right, { 1.75, -1.4, -2.75 }, MaterialAt(3));


		{
			Quad bot, back;
			bot.PositionLLC = { -5.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 10.0f;
			bot.Height = 12.5f;
			bot.Material = MaterialAt(0);

			back.PositionLLC = { -5.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 10.0f;
			back.Height = 8.0f;
			back.Material = MaterialAt(0);

			m_Quads.Add(bot);
			m_Quads.Add(back);
		}

		{
			Quad panelLight1, panelLight2;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.Material = MaterialAt(9);

			panelLight2.PositionLLC = { -9.5f, 4.5f, -0.1f, 0.0f };
			panelLight2.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight2.V = { 0.0f, 1.0f, -1.0f, 0.0f };
			panelLight2.Width = 10.0f;
			panelLight2.Height = 8.0f;
			panelLight2.Material = MaterialAt(9);

			m_Quads.Add(panelLight2);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, MaterialAt(1));
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, MaterialAt(1));

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, MaterialAt(1));
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, MaterialAt(1));

	}

	// Scena di esempio per il modello Sponza (crytek/jimmiebergmann): una mesh sola con
	// il suo MTL a 25 materiali + texture .tga, montata via UploadModel (che registra
	// materiali e texture e rimappa gli indici per-triangolo).
	void World::PrepareSponza() {
		// L'OBJ e' in unita' enormi (~3700 di larghezza): lo scalo a ~37 unita', in linea
		// con le altre scene. Sponza non ha materiali emissivi: la luce viene dal Sole, che
		// entra dal cortile aperto (ADR 0006), e da un cielo debole che riempie le ombre. Il
		// cortile e' stretto e alto, quindi il Sole lo raggiunge solo da un'elevazione alta.
		m_BackgroundColor = glm::vec3(0.12f);
		m_Sun.Enabled = true;
		m_Sun.Elevation = 75.0f;
		m_Sun.Azimuth = 100.0f;
		// L'inquadratura nel cortile al sole: quella di default della vecchia app (-11, -0.2, 0)
		// stava sotto il portico, al buio.
		m_Camera.Position = { -9.0f, 1.5f, 0.8f };
		m_Camera.Direction = glm::normalize(glm::vec3(1.0f, 0.12f, -0.1f));

		Model sponza;
		sponza.LoadObj("models/Sponza/sponza.obj");

		// il material di fallback (0) serve solo ai triangoli senza usemtl; qui quasi tutti
		// ce l'hanno, ma 0 e' comunque un indice MTL valido dopo la registrazione.
		// l'asset prima del materiale: lo 0 esiste solo dopo che il MTL e' stato registrato
		const ObjectId sponzaAsset = ObjAsset(sponza);
		AddInstance(sponzaAsset, glm::mat4(1.0f), MaterialAt(0));

		// I _ddn di Sponza sono in convenzione DirectX (verde verso il basso): sopra ogni fuga dei
		// mattoni il verde e' positivo. L'MTL non lo dice, quindi lo fissa la scena.
		for (Material& m : m_Assets.Materials)
			if (!m.NormalTexture.IsNil())
				m.NormalFlipY = 1.0f;

		// scala 0.01 + centratura su X/Z (il pavimento resta a y ~ -1.26)
		const int meshIndex = static_cast<int>(m_Meshes.size()) - 1;
		if (meshIndex >= 0) {
			glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.0f, 0.4f));
			t = glm::scale(t, glm::vec3(0.01f));
			m_Meshes.at(meshIndex).Transform = t;
		}
	}

	Material World::MaterialFromMtl(const MtlMaterial& mm) {
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
			m.AlbedoTexture = m_Assets.RegisterTexture(mm.map_Kd, TextureKind::Color);
			m.Color = glm::vec4(1.0f);
		}
		if (!mm.map_Bump.empty())
			m.NormalTexture = m_Assets.RegisterTexture(mm.map_Bump, TextureKind::Data);
		// Molte copie di Sponza citano un _mask.tga che non distribuiscono e tengono la maschera
		// nell'alpha della diffusa: se il file manca si usa quella (AlphaTexture nil).
		if (!mm.map_d.empty()) {
			if (std::filesystem::exists(mm.map_d))
				m.AlphaTexture = m_Assets.RegisterTexture(mm.map_d, TextureKind::Data);
			m.AlphaCutoff = 0.5f;
		}
		return m;
	}

	// L'asset di un OBJ caricato (issue #26): se la sorgente c'e' gia' si riusa, cosi' due istanze
	// dividono triangoli e BLAS e i materiali MTL non si duplicano. Altrimenti i materiali del MTL
	// diventano gli slot dell'asset, e i triangoli citano lo slot con il loro indice locale.
	ObjectId World::ObjAsset(const Model& model) {
		if (const auto* existing = m_Assets.FindMesh({ model.GetPath() }))
			return existing->Id;

		MeshAsset asset;
		asset.Source = { model.GetPath() };
		for (const MtlMaterial& mm : model.GetMaterials())
			asset.MaterialSlots.push_back(m_Assets.Materials.Add(MaterialFromMtl(mm), mm.Name).Id);

		asset.LocalMin = glm::vec4(1e30f);
		asset.LocalMax = glm::vec4(-1e30f);
		for (const Triangle* t : model.GetTriangles()) {
			asset.LocalMin = glm::min(asset.LocalMin, glm::min(t->A, glm::min(t->B, t->C)));
			asset.LocalMax = glm::max(asset.LocalMax, glm::max(t->A, glm::max(t->B, t->C)));
			asset.Triangles.push_back(*t);
		}

		std::cout << "Model loaded: " << asset.Triangles.size() << " triangles, "
			<< asset.MaterialSlots.size() << " MTL materials" << std::endl;
		return m_Assets.Meshes.Add(asset, model.GetName()).Id;
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

	void World::UploadModel(const Model& model, const glm::vec3& position, const ObjectId& material) {
		if (model.GetTriangles().empty()) {
			std::cerr << "Model upload skipped: no triangles were loaded." << std::endl;
			return;
		}
		AddInstance(ObjAsset(model), glm::translate(glm::mat4(1.0f), position), material);
	}

	// Un'istanza di un OBJ a runtime. Il file si legge solo se la sua geometria non c'e' ancora.
	int World::AddMesh(const std::string& objPath, const glm::vec3& position, const ObjectId& material) {
		ObjectId asset;
		if (const auto* existing = m_Assets.FindMesh({ objPath })) {
			asset = existing->Id;
		} else {
			Model model;
			model.LoadObj(objPath.c_str());
			if (model.GetTriangles().empty()) {
				std::cerr << "AddMesh failed: " << objPath << std::endl;
				return -1;
			}
			asset = ObjAsset(model);
			m_Changes |= MaterialsChanged | TexturesChanged; // i materiali del suo MTL
		}
		return AddInstance(asset, glm::translate(glm::mat4(1.0f), position), material);
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

	void World::CreateCornellBox() {
		// left green
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, -2.0f, 6.0f, 0.0f };
			quad.U = { 0.0f, 0.0f, -1.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 6.0f;
			quad.Height = 4.0f;
			quad.Material = MaterialAt(5);

			m_Quads.Add(quad);
		}

		// back white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, -2.0f, 0.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 4.0f;
			quad.Material = MaterialAt(4);

			m_Quads.Add(quad);
		}

		// right red
		{
			Quad quad;
			quad.PositionLLC = { 2.0f, -2.0f, 0.0f, 0.0f };
			quad.U = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 6.0f;
			quad.Height = 4.0f;
			quad.Material = MaterialAt(0);

			m_Quads.Add(quad);
		}

		// top white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, 2.0f, 0.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 6.0f;
			quad.Material = MaterialAt(4);

			m_Quads.Add(quad);
		}

		// bot white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, -2.0f, 6.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 6.0f;
			quad.Material = MaterialAt(4);

			m_Quads.Add(quad);
		}

		// top light
		{
			Quad quad;
			quad.PositionLLC = { -0.25f, 1.95f, 2.00f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.Width = 0.5f;
			quad.Height = 0.5f;
			quad.Material = MaterialAt(3);

			m_Quads.Add(quad);
		}
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

	ObjectId World::MaterialAt(float index) const {
		const int i = static_cast<int>(index);
		if (i < 0 || i >= static_cast<int>(m_Assets.Materials.size())) {
			std::cerr << "MaterialAt: nessun materiale " << i << std::endl;
			return {};
		}
		return m_Assets.Materials[i].Id;
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
