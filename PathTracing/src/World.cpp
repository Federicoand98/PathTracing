#include "World.h"
#include "Renderer/Material.h"

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
		Meshes.clear();
		Boxes.clear();
		BVHNodes.clear();
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

		//CreateBox({ 0,0,0 }, { 3,1,1 }, 1);

		Model m;
		m.LoadObj("models/guitar.obj");

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

		Materials.push_back(dielectric);
		Materials.push_back(metal);
		Materials.push_back(diffuse);
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
		Material metal1 = CreateDefaultGlossy({ 0.8549, 0.5490, 0.4, 1.0 }, 0.5, 0.3);
		Material metal2 = CreateDefaultGlossy({ 0.8549, 0.5490, 0.4, 1.0 }, 0.5, 0.6);
		Material metal3 = CreateDefaultGlossy({ 0.8549, 0.5490, 0.4, 1.0 }, 0.5, 1);

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

	void World::UploadModel(const Model& model, const glm::vec3& Position, int material) {
		std::vector<Triangle*> triangles = model.GetTriangles();

		if (triangles.empty()) {
			std::cerr << "Model upload skipped: no triangles were loaded." << std::endl;
			return;
		}

		MeshInfo mesh;
		mesh.Position = glm::vec4(0.0f); // vertici bakati in world space, nessun offset a runtime
		mesh.FirstTriangle = static_cast<float>(Triangles.size());
		mesh.NumTriangles = static_cast<float>(triangles.size());
		mesh.MaterialIndex = static_cast<float>(material);

		const glm::vec4 offset = glm::vec4(Position, 0.0f);
		glm::vec4 boundsMin(1e30f);
		glm::vec4 boundsMax(-1e30f);

		// accumula i triangoli (in world space) nella collezione globale della scena
		for (const Triangle* t : triangles) {
			Triangle tri = *t;
			tri.A += offset;
			tri.B += offset;
			tri.C += offset;

			boundsMin = glm::min(boundsMin, glm::min(tri.A, glm::min(tri.B, tri.C)));
			boundsMax = glm::max(boundsMax, glm::max(tri.A, glm::max(tri.B, tri.C)));

			Triangles.push_back(tri);
		}

		mesh.BoundsMin = boundsMin;
		mesh.BoundsMax = boundsMax;

		Meshes.push_back(mesh);

		std::cout << "Model uploaded: " << triangles.size() << " triangles (material "
			<< material << ")" << std::endl;
	}

	void World::BuildBVH() {
		if (Triangles.empty()) {
			BVHNodes.clear();
			TriIndex.clear();
			return;
		}

		BVH builder(Triangles);
		BVHNodes = builder.GetNodes();
		TriIndex = builder.GetTrianglesIndices();

		// La traversata nello shader usa uno stack fisso (MAX_STACK): se l'albero e'
		// piu' profondo, i nodi in eccesso vengono scartati e la mesh mostra dei buchi.
		int maxDepth = 0;
		std::vector<std::pair<int, int>> stack{ {0, 1} }; // (indice nodo, profondita')
		while (!stack.empty()) {
			auto [nodeIndex, depth] = stack.back();
			stack.pop_back();
			maxDepth = std::max(maxDepth, depth);

			const BVHNodeNew& node = BVHNodes.at(nodeIndex);
			if (node.triCount == 0.0f) { // nodo interno
				stack.push_back({ static_cast<int>(node.left), depth + 1 });
				stack.push_back({ static_cast<int>(node.left) + 1, depth + 1 });
			}
		}

		std::cout << "BVH built: " << Triangles.size() << " triangles, "
			<< BVHNodes.size() << " nodes, max depth " << maxDepth << std::endl;
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
