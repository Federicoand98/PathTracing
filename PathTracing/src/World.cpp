#include "World.h"
#include "Renderer/Material.h"
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace PathTracer {

	World::World() {
		BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);

        for(int i = 0; i < 10; i++) {
            Tests.push_back({glm::vec3(1, 0, 0), glm::vec3(0, 0, 1)});
        }
	}

	World::~World() {
		Spheres.clear();
		Materials.clear();
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

		// un unico BVH globale su tutti i triangoli della scena
		BuildBVH();
	}

	void World::DestroyScene() {
		Spheres.clear();
		Quads.clear();
		Materials.clear();
		Triangles.clear();
		TriPositions.clear();
		TriNormals.clear();
		TriUVs.clear();
		TexturePaths.clear();
		Meshes.clear();
		MeshNames.clear();
		MeshPaths.clear();
		Boxes.clear();
		BVH4Nodes.clear();
		TriIndex.clear();
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

		Materials.push_back(redMaterial);
		Materials.push_back(blueMaterial);
		Materials.push_back(metal);
		Materials.push_back(lightMaterial);
		Materials.push_back(whiteMaterial);
		Materials.push_back(greenMaterial);
		Materials.push_back(glassMaterial);
	}

	void World::PrepareSimpleScene() {
		BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
		{
			Sphere s;
			s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
			s.MaterialIndex = 2;
			// Spheres.push_back(s);
		}
		{
			Sphere s;
			s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
			s.MaterialIndex = 3;
			// Spheres.push_back(s);
		}
		{
			Sphere s;
			s.Position = { 0.0f, -101.0f, 0.0f, 100.0f };
			s.MaterialIndex = 0;
			Spheres.push_back(s);
		}

		// Sfera gigante usata come terreno: scacchiera bianco/nero in world space.
		// Una scacchiera sulle UV non andrebbe: la camera sta quasi sul polo della sfera,
		// dove la mappatura equirettangolare degenera in spicchi convergenti.
		{
			Material& ground = Materials.at(0); // in questa scena il materiale 0 e' solo suo
			ground.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
			ground.SpecularColor = ground.Color;
			ground.Roughness = 1.0f;
			ground.SpecularProbability = 0.0f; // terreno opaco: niente riflesso speculare
			ground.Checker = 2.0f;             // world-space
			ground.CheckerScale = 0.5f;        // celle da 2 unita'
		}

		//CreateBox({ 0,0,0 }, { 3,1,1 }, 1);

		Model m;
		m.LoadObj("models/guitar.obj");

		// layer 0 del sampler2DArray
		TexturePaths.push_back("textures/Albedo/uv_grid.png");

		// l'albedo moltiplica material.Color: se il materiale e' colorato la texture
		// ne esce tinta, quindi lo si porta a bianco
		Material& guitarMaterial = Materials.at(5);
		guitarMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		guitarMaterial.SpecularColor = guitarMaterial.Color;
		guitarMaterial.AlbedoTexture = 0.0f;

		UploadModel(m, { 0.0, 0.0, 0.0 }, 5);
	}

	void World::PrepareCornellBox() {
		BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
		{
			Sphere s;
			s.Position = { -0.9f, -1.2f, 1.5f, 0.8f };
			s.MaterialIndex = 2;
			Spheres.push_back(s);
		}
		{
			Sphere s;
			s.Position = { 0.0f, -1.6f, 2.3f, 0.4f };
			s.MaterialIndex = 2;
			Spheres.push_back(s);
		}
		{
			Sphere s;
			s.Position = { 0.9f, -1.0f, 1.1f, 1.0f };
			s.MaterialIndex = 2;
			Spheres.push_back(s);
		}

		CreateCornellBox();
	}

	void World::PrepareCornellBoxMesh() {
		BackgroundColor = { 0.54f, 0.73f, 0.95f };
		CreateCornellBox();

		Model queen, king, knight;
		queen.LoadObj("models/queen.obj");
		king.LoadObj("models/king.obj");
		knight.LoadObj("models/knight.obj");

		UploadModel(queen, { 0.7, -2.1, 2.0 }, 2);
		UploadModel(king, { -1.2, -2.0, 1.6 }, 5);
		UploadModel(knight, { 0.9, -2.0, 3.0 }, 1);
	}

	void World::PrepareRandomSpheres() {
		BackgroundColor = { 0.54f, 0.73f, 0.95f };

		Sphere base = { {0.0f, -1001.0f, 0.0f, 1000.0f}, 3.0f };
		Sphere sphere1 = { {0.0f, 0.0f, 0.0f, 1.0f}, 0.0f };
		Sphere sphere2 = { {-3.0f, 0.0f, 0.0f, 1.0f}, 2.0f };
		Sphere sphere3 = { {3.0f, 0.0f, 0.0f, 1.0f}, 1.0f };

		Material dielectric, metal, diffuse, baseMat;
		dielectric = CreateDefaultDielectric();
		metal = CreateDefaultMetal();
		diffuse = CreateDefaultDiffuse();
		baseMat.Color = { 0.5f, 0.5f, 0.5f, 1.0f };
		baseMat.Roughness = 0.8f;
		baseMat.SpecularColor = baseMat.Color;

		// layer 0 del sampler2DArray. L'albedo moltiplica material.Color: un materiale
		// colorato tingerebbe la texture, quindi il colore va portato a bianco.
		TexturePaths.push_back("textures/earthmap.jpg");
		diffuse.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		diffuse.SpecularColor = diffuse.Color;
		diffuse.SpecularProbability = 0.0f; // niente riflesso: la texture si vedrebbe poco
		diffuse.AlbedoTexture = 0.0f;

		Materials.push_back(dielectric);
		Materials.push_back(metal);
		Materials.push_back(diffuse);   // materiale 2: solo di sphere2, la sfera "terra"
		Materials.push_back(baseMat);
		Spheres.push_back(sphere1);
		Spheres.push_back(sphere2);
		Spheres.push_back(sphere3);
		Spheres.push_back(base);

		for (int a = -11; a < 11; a++) {
			for (int b = -11; b < 11; b++) {
				glm::vec3 center(a + 0.9 * Random::GetFloat(0, 1), -0.82f, b + 0.9 * Random::GetFloat(0, 1));

				if (glm::length(center - glm::vec3(3, 0.2, 0)) > 0.9) {
					Material mat;
					mat = CreateRandom(true);
					Materials.push_back(mat);

					Sphere s = { {center, 0.2f}, 0.0 };
					s.MaterialIndex = Materials.size() - 1;

					Spheres.push_back(s);
				}
			}
		}

		/*
		for (int i = 4; i < Spheres.size(); i++) {
			Spheres.at(i).MaterialIndex = Random::GetFloat(3, Materials.size() - 1);
		}
		*/
	}

	void World::PrepareRandomBoxes() {
		BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
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

		Materials.push_back(dielectric);
		Materials.push_back(metal);
		Materials.push_back(diffuse);
		Materials.push_back(lightMaterial);
		Materials.push_back(baseMat);

		{
			Sphere s;
			s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
			s.MaterialIndex = 1;
			Spheres.push_back(s);
		}
		{
			Sphere s;
			s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
			s.MaterialIndex = 3;
			Spheres.push_back(s);
		}

		for (int a = 0; a < 10; a++) {
			Material mat;
			mat = CreateRandom(false);

			Materials.push_back(mat);
		}

		for (int x = -5; x < 6; x++) {
			for (int z = -5; z < 6; z++) {
				float y = Random::GetFloat(-4, -3);

				CreateBox({ x, y, z }, { x + 1, y + 1, z + 1 }, Random::GetInt(5, Materials.size() - 1));
			}
		}
	}

	void World::PrepareSetup1() {
		BackgroundColor = { 0.0, 0.0, 0.0 };

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

		Materials.push_back(white_diffuse);
		Materials.push_back(metal);
		Materials.push_back(red);
		Materials.push_back(blue);
		Materials.push_back(green);
		Materials.push_back(white_metal);
		Materials.push_back(white_glossy);
		Materials.push_back(glass1);
		Materials.push_back(glass2);
		Materials.push_back(light);
		Materials.push_back(metal1);
		Materials.push_back(metal2);
		Materials.push_back(metal3);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.MaterialIndex = 0;

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.MaterialIndex = 6;

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.MaterialIndex = 5;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.MaterialIndex = 2;

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.MaterialIndex = 3;

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.MaterialIndex = 4;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.MaterialIndex = 10;

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.MaterialIndex = 11;

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.MaterialIndex = 12;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, right;
			left.Position = { -1.75f, -0.9f, -2.75f, 0.4f };
			left.MaterialIndex = 8;

			right.Position = { 1.75f, -0.9f, -2.75f, 0.4f };
			right.MaterialIndex = 7;

			Spheres.push_back(left);
			Spheres.push_back(right);
		}

		{
			Quad bot, back;
			bot.PositionLLC = { -10.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 20.0f;
			bot.Height = 12.5f;
			bot.MaterialIndex = 0;

			back.PositionLLC = { -10.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 20.0f;
			back.Height = 8.0f;
			back.MaterialIndex = 0;

			Quads.push_back(bot);
			Quads.push_back(back);
		}

		{
			Quad panelLight1, panelLight2;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.MaterialIndex = 9;

			panelLight2.PositionLLC = { -9.5f, 4.5f, -0.1f, 0.0f };
			panelLight2.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight2.V = { 0.0f, 1.0f, -1.0f, 0.0f };
			panelLight2.Width = 10.0f;
			panelLight2.Height = 8.0f;
			panelLight2.MaterialIndex = 9;

			Quads.push_back(panelLight2);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, 1);
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, 1);

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, 1);
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, 1);
	}

	void World::PrepareSetup2() {
		BackgroundColor = { 0.0, 0.0, 0.0 };

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

		Materials.push_back(white_diffuse);
		Materials.push_back(metal);
		Materials.push_back(red);
		Materials.push_back(blue);
		Materials.push_back(green);
		Materials.push_back(white_metal);
		Materials.push_back(white_glossy);
		Materials.push_back(glass1);
		Materials.push_back(glass2);
		Materials.push_back(light);
		Materials.push_back(lightR);
		Materials.push_back(lightG);
		Materials.push_back(lightB);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.MaterialIndex = 0;

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.MaterialIndex = 6;

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.MaterialIndex = 5;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.MaterialIndex = 2;

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.MaterialIndex = 3;

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.MaterialIndex = 4;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		/*
		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.MaterialIndex = 9;

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.MaterialIndex = 9;

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.MaterialIndex = 9;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}
		*/

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -3.0f, 0.5f };
			left.MaterialIndex = 10;

			middle.Position = { 0.0f, 2.2f, -3.0f, 0.5f };
			middle.MaterialIndex = 12;

			right.Position = { 3.5f, 2.2f, -3.0f, 0.5f };
			right.MaterialIndex = 11;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, right;
			left.Position = { -1.75f, -0.9f, -2.75f, 0.4f };
			left.MaterialIndex = 8;

			right.Position = { 1.75f, -0.9f, -2.75f, 0.4f };
			right.MaterialIndex = 7;

			Spheres.push_back(left);
			Spheres.push_back(right);
		}


		{
			Quad bot, back;
			bot.PositionLLC = { -10.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 20.0f;
			bot.Height = 12.5f;
			bot.MaterialIndex = 0;

			back.PositionLLC = { -10.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 20.0f;
			back.Height = 8.0f;
			back.MaterialIndex = 0;

			Quads.push_back(bot);
			Quads.push_back(back);
		}

		{
			Quad panelLight1;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.MaterialIndex = 9;

			Quads.push_back(panelLight1);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, 1);
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, 1);

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, 1);
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, 1);
	}

	void World::PrepareSetup3() {
		BackgroundColor = { 0.0, 0.0, 0.0 };

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

		Materials.push_back(white_diffuse);
		Materials.push_back(metal);
		Materials.push_back(red);
		Materials.push_back(blue);
		Materials.push_back(green);
		Materials.push_back(white_metal);
		Materials.push_back(white_glossy);
		Materials.push_back(glass1);
		Materials.push_back(glass2);
		Materials.push_back(light);

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -0.5f, -4.5f, 1.0f };
			left.MaterialIndex = 0;

			middle.Position = { 0.0f, -0.5f, -4.5f, 1.0f };
			middle.MaterialIndex = 6;

			right.Position = { 3.5f, -0.5f, -4.5f, 1.0f };
			right.MaterialIndex = 5;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, -1.0f, -2.0f, 0.5f };
			left.MaterialIndex = 2;

			middle.Position = { 0.0f, -1.0f, -2.0f, 0.5f };
			middle.MaterialIndex = 3;

			right.Position = { 3.5f, -1.0f, -2.0f, 0.5f };
			right.MaterialIndex = 4;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		{
			Sphere left, middle, right;
			left.Position = { -3.5f, 2.2f, -5.5f, 0.5f };
			left.MaterialIndex = 0;

			middle.Position = { 0.0f, 2.2f, -5.5f, 0.5f };
			middle.MaterialIndex = 0;

			right.Position = { 3.5f, 2.2f, -5.5f, 0.5f };
			right.MaterialIndex = 0;

			Spheres.push_back(left);
			Spheres.push_back(middle);
			Spheres.push_back(right);
		}

		Model left, right;
		left.LoadObj("models/king.obj");
		right.LoadObj("models/queen.obj");

		UploadModel(left, { -1.75, -1.4, -2.75 }, 3);
		UploadModel(right, { 1.75, -1.4, -2.75 }, 3);


		{
			Quad bot, back;
			bot.PositionLLC = { -5.0f, -1.5f, 2.5f, 0.0f };
			bot.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			bot.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			bot.Width = 10.0f;
			bot.Height = 12.5f;
			bot.MaterialIndex = 0;

			back.PositionLLC = { -5.0f, -1.5f, -10.0f, 0.0f };
			back.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			back.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			back.Width = 10.0f;
			back.Height = 8.0f;
			back.MaterialIndex = 0;

			Quads.push_back(bot);
			Quads.push_back(back);
		}

		{
			Quad panelLight1, panelLight2;

			panelLight1.PositionLLC = { -9.5f, -1.5f, -0.1f, 0.0f };
			panelLight1.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight1.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			panelLight1.Width = 10.0f;
			panelLight1.Height = 8.0f;
			panelLight1.MaterialIndex = 9;

			panelLight2.PositionLLC = { -9.5f, 4.5f, -0.1f, 0.0f };
			panelLight2.U = { 1.0f, 0.0f, 1.0f, 0.0f };
			panelLight2.V = { 0.0f, 1.0f, -1.0f, 0.0f };
			panelLight2.Width = 10.0f;
			panelLight2.Height = 8.0f;
			panelLight2.MaterialIndex = 9;

			Quads.push_back(panelLight2);
		}

		// SLAB
		CreateBox({ -2.5, -1.5f, -3.5 }, { -1.0, -1.3f, -2.0 }, 1);
		CreateBox({ 1.0, -1.5f, -3.5 }, { 2.5, -1.3f, -2.0 }, 1);

		// MONOLITH
		CreateBox({ -3.5, -1.5f, -7.3 }, { -2.0, 3.2f, -7.0 }, 1);
		CreateBox({ 2.0, -1.5f, -7.3 }, { 3.5, 3.2f, -7.0 }, 1);

	}

	// Scena di esempio per il modello Sponza (crytek/jimmiebergmann): una mesh sola con
	// il suo MTL a 25 materiali + texture .tga, montata via UploadModel (che registra
	// materiali e texture e rimappa gli indici per-triangolo).
	void World::PrepareSponza() {
		// L'OBJ e' in unita' enormi (~3700 di larghezza): lo scalo a ~37 unita', in linea
		// con le altre scene. Sponza non ha materiali emissivi, quindi la luce arriva tutta
		// dal cielo: l'atrio e' aperto in alto, un background luminoso lo illumina in modo
		// naturale (i raggi che mancano la geometria ritornano BackgroundColor come radianza).
		BackgroundColor = glm::vec3(1.0f, 1.0f, 1.0f);

		Model sponza;
		sponza.LoadObj("models/Sponza/sponza.obj");

		// il material di fallback (0) serve solo ai triangoli senza usemtl; qui quasi tutti
		// ce l'hanno, ma 0 e' comunque un indice MTL valido dopo la registrazione.
		UploadModel(sponza, { 0.0f, 0.0f, 0.0f }, 0);

		// scala 0.01 + centratura su X/Z (il pavimento resta a y ~ -1.26)
		const int meshIndex = static_cast<int>(Meshes.size()) - 1;
		if (meshIndex >= 0) {
			glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.6f, 0.0f, 0.4f));
			t = glm::scale(t, glm::vec3(0.01f));
			SetMeshTransform(meshIndex, t);
		}
	}

	// Un layer del sampler2DArray per path, deduplicato: due materiali con la stessa
	// texture condividono il layer.
	int World::RegisterTexture(const std::string& path) {
		for (size_t i = 0; i < TexturePaths.size(); i++)
			if (TexturePaths[i] == path)
				return static_cast<int>(i);
		TexturePaths.push_back(path);
		return static_cast<int>(TexturePaths.size()) - 1;
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
			m.AlbedoTexture = static_cast<float>(RegisterTexture(mm.map_Kd));
			m.Color = glm::vec4(1.0f);
		}
		return m;
	}

	// Appende i materiali del MTL in coda a Materials e ritorna l'indice del primo. I triangoli
	// del modello portano indici LOCALI (0..N-1): il chiamante li somma a questa base.
	int World::RegisterMtlMaterials(const Model& model) {
		const std::vector<MtlMaterial>& mtls = model.GetMaterials();
		if (mtls.empty())
			return -1;

		const int base = static_cast<int>(Materials.size());
		for (const MtlMaterial& mm : mtls)
			Materials.push_back(MaterialFromMtl(mm));

		std::cout << "MTL: registrati " << mtls.size() << " materiali (base " << base << ")"
			<< ", texture totali " << TexturePaths.size() << std::endl;
		return base;
	}

	void World::UploadModel(const Model& model, const glm::vec3& Position, int material) {
		std::vector<Triangle*> triangles = model.GetTriangles();

		if (triangles.empty()) {
			std::cerr << "Model upload skipped: no triangles were loaded." << std::endl;
			return;
		}

		// OBJ multi-materiale: registra i materiali del MTL e ottieni l'indice base a cui
		// rimappare gli indici locali dei triangoli. -1 = OBJ senza MTL (comportamento legacy).
		const int matBase = RegisterMtlMaterials(model);

		MeshInfo mesh;
		mesh.FirstTriangle = static_cast<float>(Triangles.size());
		mesh.NumTriangles = static_cast<float>(triangles.size());
		mesh.MaterialIndex = static_cast<float>(material); // usato dai triangoli senza usemtl (-1)

		glm::vec4 localMin(1e30f);
		glm::vec4 localMax(-1e30f);

		// i triangoli restano in LOCAL SPACE: la posizione vive nella Transform
		for (const Triangle* t : triangles) {
			Triangle tri = *t;

			// rimappa l'indice materiale da locale (nel MTL) a globale (in Materials).
			// -1 resta -1 e a valle diventa il fallback al materiale della mesh.
			if (matBase >= 0 && tri.MaterialIndex >= 0.0f)
				tri.MaterialIndex = static_cast<float>(matBase + static_cast<int>(tri.MaterialIndex));

			localMin = glm::min(localMin, glm::min(tri.A, glm::min(tri.B, tri.C)));
			localMax = glm::max(localMax, glm::max(tri.A, glm::max(tri.B, tri.C)));

			Triangles.push_back(tri);
		}

		mesh.LocalMin = localMin;
		mesh.LocalMax = localMax;

		Meshes.push_back(mesh);
		MeshNames.push_back(model.GetName());
		MeshPaths.push_back(model.GetPath());
		SetMeshTransform(static_cast<int>(Meshes.size()) - 1, glm::translate(glm::mat4(1.0f), Position));

		std::cout << "Model uploaded: " << triangles.size() << " triangles (material "
			<< material << ")" << std::endl;
	}

	// Carica un OBJ a runtime. I BLAS vivono tutti in BVH4Nodes con offset per mesh,
	// quindi aggiungerne uno impone di ricostruire l'intero BVH: gli offset dei nodi
	// e gli indici in TriIndex cambiano. Costoso, ma e' un'azione esplicita dell'utente.
	int World::AddMesh(const std::string& objPath, const glm::vec3& position, int material) {
		Model model;
		model.LoadObj(objPath.c_str());

		if (model.GetTriangles().empty()) {
			std::cerr << "AddMesh failed: " << objPath << std::endl;
			return -1;
		}

		UploadModel(model, position, material);
		BuildBVH();

		return static_cast<int>(Meshes.size()) - 1;
	}

	int World::QuadOwnerBox(int quadIndex) const {
		for (size_t b = 0; b < Boxes.size(); b++) {
			const int first = static_cast<int>(Boxes[b].index);
			if (quadIndex >= first && quadIndex < first + 6)
				return static_cast<int>(b);
		}
		return -1;
	}

	// Nessuno tiene indici verso le sfere, a parte la selezione: qui basta la erase.
	bool World::RemoveSphere(int sphereIndex) {
		if (sphereIndex < 0 || sphereIndex >= static_cast<int>(Spheres.size()))
			return false;

		Spheres.erase(Spheres.begin() + sphereIndex);
		return true;
	}

	// I Box puntano ai quad per indice (Box::index = primo dei suoi 6, consecutivi).
	// Togliere un quad davanti a un box gli sposta i piedi sotto: va rimappato.
	// Lo shader scorre Quads e Cubes in lockstep e pretende FirstQuad crescente,
	// quindi la rimappatura per sottrazione conserva anche l'ordinamento.
	bool World::RemoveQuad(int quadIndex) {
		if (quadIndex < 0 || quadIndex >= static_cast<int>(Quads.size()))
			return false;

		// un quad di un box non e' un oggetto autonomo: si cancella il box, non la faccia
		if (QuadOwnerBox(quadIndex) >= 0)
			return false;

		Quads.erase(Quads.begin() + quadIndex);

		for (Box& box : Boxes)
			if (static_cast<int>(box.index) > quadIndex)
				box.index -= 1.0f;

		return true;
	}

	bool World::RemoveBox(int boxIndex) {
		if (boxIndex < 0 || boxIndex >= static_cast<int>(Boxes.size()))
			return false;

		const int first = static_cast<int>(Boxes[boxIndex].index);
		if (first < 0 || first + 6 > static_cast<int>(Quads.size()))
			return false;

		Quads.erase(Quads.begin() + first, Quads.begin() + first + 6);
		Boxes.erase(Boxes.begin() + boxIndex);

		for (Box& box : Boxes)
			if (static_cast<int>(box.index) > first)
				box.index -= 6.0f;

		return true;
	}

	// I triangoli di tutte le mesh vivono concatenati in Triangles, e ogni MeshInfo ci
	// punta con FirstTriangle. Togliendone una si apre un buco: vanno rimossi i suoi
	// triangoli e va sottratto il conteggio a tutte le mesh che stavano dopo, altrimenti
	// le rimanenti indicherebbero triangoli altrui. Poi BuildBVH() rifa' tutto il resto
	// (TriPositions/Normals/UVs, TriIndex, BVH4Nodes, RootNode) da Triangles + Meshes.
	bool World::RemoveMesh(int meshIndex) {
		if (meshIndex < 0 || meshIndex >= static_cast<int>(Meshes.size()))
			return false;

		const int first = static_cast<int>(Meshes[meshIndex].FirstTriangle);
		const int count = static_cast<int>(Meshes[meshIndex].NumTriangles);

		Triangles.erase(Triangles.begin() + first, Triangles.begin() + first + count);

		Meshes.erase(Meshes.begin() + meshIndex);
		if (meshIndex < static_cast<int>(MeshNames.size()))
			MeshNames.erase(MeshNames.begin() + meshIndex);
		if (meshIndex < static_cast<int>(MeshPaths.size()))
			MeshPaths.erase(MeshPaths.begin() + meshIndex);

		for (int i = meshIndex; i < static_cast<int>(Meshes.size()); i++)
			Meshes[i].FirstTriangle -= static_cast<float>(count);

		BuildBVH();

		std::cout << "Mesh " << meshIndex << " removed (" << count << " triangles)" << std::endl;
		return true;
	}

	// Aggiorna la trasformazione di una mesh: ricalcola l'inversa e l'AABB world.
	// Nessun triangolo viene toccato, nessun BVH ricostruito.
	void World::SetMeshTransform(int meshIndex, const glm::mat4& transform) {
		MeshInfo& mesh = Meshes.at(meshIndex);
		mesh.Transform = transform;
		mesh.InvTransform = glm::inverse(transform);

		// AABB world = box che racchiude gli 8 angoli locali trasformati
		glm::vec3 worldMin(1e30f), worldMax(-1e30f);
		for (int corner = 0; corner < 8; corner++) {
			glm::vec4 p(
				(corner & 1) ? mesh.LocalMax.x : mesh.LocalMin.x,
				(corner & 2) ? mesh.LocalMax.y : mesh.LocalMin.y,
				(corner & 4) ? mesh.LocalMax.z : mesh.LocalMin.z,
				1.0f);

			glm::vec3 w = glm::vec3(transform * p);
			worldMin = glm::min(worldMin, w);
			worldMax = glm::max(worldMax, w);
		}

		mesh.BoundsMin = glm::vec4(worldMin, 0.0f);
		mesh.BoundsMax = glm::vec4(worldMax, 0.0f);
	}

	void World::BuildBVH() {
		const auto buildStart = std::chrono::high_resolution_clock::now();

		BVH4Nodes.clear();
		TriIndex.clear();
		TriPositions.clear();
		TriNormals.clear();
		TriUVs.clear();

		if (Triangles.empty())
			return;

		// separa posizioni, normali e UV nei buffer che finiranno sulla GPU (local space)
		TriPositions.reserve(Triangles.size());
		TriNormals.reserve(Triangles.size());
		TriUVs.reserve(Triangles.size());
		for (const Triangle& t : Triangles) {
			TriPositions.push_back({ t.A, t.B, t.C });
			TriNormals.push_back({ t.NormalA, t.NormalB, t.NormalC });
			// A.z porta il materiale per-triangolo (-1 = eredita dalla mesh); B/C restano liberi
			TriUVs.push_back({ glm::vec4(t.UVA, t.MaterialIndex, 0.0f), glm::vec4(t.UVB, 0.0f, 0.0f), glm::vec4(t.UVC, 0.0f, 0.0f) });
		}

		// Un BLAS per mesh. Ogni BLAS si costruisce binario (binned SAH) e poi si COLLASSA
		// in un BVH a 4 vie: meta' dei livelli, meno fetch di nodi per raggio. I nodi wide
		// e gli indici sono locali al BLAS: qui vengono concatenati e rimappati ai globali.
		int maxDepth = 0;

		for (MeshInfo& mesh : Meshes) {
			const int first = static_cast<int>(mesh.FirstTriangle);
			const int count = static_cast<int>(mesh.NumTriangles);
			if (count == 0) continue;

			SBVH builder(Triangles, first, count);
			int wideDepth = 0;
			std::vector<BVH4Node> wide = WideBVH::Collapse(builder.GetNodes(), wideDepth);

			const int nodeOffset = static_cast<int>(BVH4Nodes.size());
			const int triOffset = static_cast<int>(TriIndex.size());

			mesh.RootNode = static_cast<float>(nodeOffset);

			for (BVH4Node node : wide) {
				for (int c = 0; c < 4; c++) {
					if (node.count[c] < 0) continue;               // slot vuoto
					if (node.count[c] == 0) node.child[c] += nodeOffset; // figlio interno
					else node.child[c] += triOffset;               // foglia: primo indice in TriIndex
				}
				BVH4Nodes.push_back(node);
			}

			for (int localIdx : builder.GetTrianglesIndices())
				TriIndex.push_back(first + localIdx);

			maxDepth = std::max(maxDepth, wideDepth);
		}

		const double buildMs = std::chrono::duration<double, std::milli>(
			std::chrono::high_resolution_clock::now() - buildStart).count();

		std::cout << "BVH built: " << Triangles.size() << " triangles, "
			<< TriIndex.size() << " refs (dup x"
			<< (Triangles.empty() ? 0.0f : (float)TriIndex.size() / Triangles.size()) << "), "
			<< BVH4Nodes.size() << " wide nodes, " << Meshes.size() << " BLAS, max depth "
			<< maxDepth << " in " << buildMs << " ms" << std::endl;

		// lo stack per-thread in hitBVH() dentro PathTracing.comp e' fisso a MAX_STACK.
		// Il BVH4 impila fino a 3 figli interni per nodo, quindi puo' servire piu' spazio
		// della sola profondita': tengo il margine allineato a MAX_STACK.
		constexpr int SHADER_MAX_STACK = 64;
		if (maxDepth * 3 > SHADER_MAX_STACK)
			std::cerr << "Warning: BVH depth (" << maxDepth << ") too deep for shader stack ("
				<< SHADER_MAX_STACK << "): parts of the mesh may not render" << std::endl;
	}

	void World::CreateBox(const glm::vec3& a, const glm::vec3& b, float MaterialIndex) {
		Box box;
		box.Min = glm::vec4(fmin(a.x, b.x), fmin(a.y, b.y), fmin(a.z, b.z), 0.0f);
		box.Max = glm::vec4(fmax(a.x, b.x), fmax(a.y, b.y), fmax(a.z, b.z), 0.0f);

		glm::vec3 dx = glm::vec3(box.Max.x - box.Min.x, 0, 0);
		glm::vec3 dy = glm::vec3(0, box.Max.y - box.Min.y, 0);
		glm::vec3 dz = glm::vec3(0, 0, box.Max.z - box.Min.z);

		glm::vec3 nx = glm::normalize(dx);
		glm::vec3 ny = glm::normalize(dy);
		glm::vec3 nz = glm::normalize(dz);

		box.index = Quads.size();
		box.MaterialIndex = MaterialIndex;

		Quad quad;

		quad.MaterialIndex = box.MaterialIndex;

		// front
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Max.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			Quads.push_back(quad);
		}

		// right
		{
			quad.PositionLLC = { box.Max.x, box.Min.y, box.Max.z, 0 };
			quad.U = { -nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			Quads.push_back(quad);
		}

		// back
		{
			quad.PositionLLC = { box.Max.x, box.Min.y, box.Min.z, 0 };
			quad.U = { -nx, 0 };
			quad.V = { ny, 0 };
			quad.Width = dx.x;
			quad.Height = dy.y;

			Quads.push_back(quad);
		}

		// left
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Min.z, 0 };
			quad.U = { nz, 0 };
			quad.V = { ny, 0 };
			quad.Width = dz.z;
			quad.Height = dy.y;

			Quads.push_back(quad);
		}

		// top
		{
			quad.PositionLLC = { box.Min.x, box.Max.y, box.Max.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { -nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			Quads.push_back(quad);
		}

		// bottom
		{
			quad.PositionLLC = { box.Min.x, box.Min.y, box.Min.z, 0 };
			quad.U = { nx, 0 };
			quad.V = { nz, 0 };
			quad.Width = dx.x;
			quad.Height = dz.z;

			Quads.push_back(quad);
		}

		Boxes.push_back(box);
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
			quad.MaterialIndex = 5;

			Quads.push_back(quad);
		}

		// back white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, -2.0f, 0.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 4.0f;
			quad.MaterialIndex = 4;

			Quads.push_back(quad);
		}

		// right red
		{
			Quad quad;
			quad.PositionLLC = { 2.0f, -2.0f, 0.0f, 0.0f };
			quad.U = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
			quad.Width = 6.0f;
			quad.Height = 4.0f;
			quad.MaterialIndex = 0;

			Quads.push_back(quad);
		}

		// top white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, 2.0f, 0.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 6.0f;
			quad.MaterialIndex = 4;

			Quads.push_back(quad);
		}

		// bot white
		{
			Quad quad;
			quad.PositionLLC = { -2.0f, -2.0f, 6.0f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, -1.0f, 0.0f };
			quad.Width = 4.0f;
			quad.Height = 6.0f;
			quad.MaterialIndex = 4;

			Quads.push_back(quad);
		}

		// top light
		{
			Quad quad;
			quad.PositionLLC = { -0.25f, 1.95f, 2.00f, 0.0f };
			quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
			quad.V = { 0.0f, 0.0f, 1.0f, 0.0f };
			quad.Width = 0.5f;
			quad.Height = 0.5f;
			quad.MaterialIndex = 3;

			Quads.push_back(quad);
		}
	}
}
