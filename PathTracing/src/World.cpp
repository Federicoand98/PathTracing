#include "World.h"

World::World() {
	BackgroundColor = glm::vec3(0.6f, 0.7f, 0.9f);
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
            PrepareRandomScene();
            break;
        default:
            break;
    }
}

void World::DestroyScene() {
    Spheres.clear();
    Materials.clear();
}

void World::PrepareMaterials() {
	Material redMaterial, blueMaterial, pinkMaterial, lightMaterial, greenMaterial, whiteMaterial, glassMaterial;
	redMaterial.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
	redMaterial.Roughness = 0.5f;
	blueMaterial.Color = { 0.0f, 0.0f, 1.0f, 1.0f };
	blueMaterial.Roughness = 0.0f;
	pinkMaterial.Color = { 1.0f, 0.0f, 1.0f, 1.0f };
	pinkMaterial.Roughness = 1.0f;
	lightMaterial.Color = { 0.88f, 0.83f, 0.3f, 1.0f };
	lightMaterial.Roughness = 1.0f;
	lightMaterial.EmissiveColor = lightMaterial.Color;
	lightMaterial.EmissiveStrenght = 1.0f;
	greenMaterial.Color = { 0.0f, 1.0f, 0.0f, 1.0f };
	greenMaterial.Roughness = 1.0f;
	whiteMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	whiteMaterial.Roughness = 1.0f;
	glassMaterial.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	glassMaterial.RefractionRatio = 1.7f;

	Materials.push_back(redMaterial);
	Materials.push_back(blueMaterial);
	Materials.push_back(pinkMaterial);
	Materials.push_back(lightMaterial);
	Materials.push_back(whiteMaterial);
	Materials.push_back(greenMaterial);
	Materials.push_back(glassMaterial);
}

void World::PrepareSimpleScene() {
	{
		Sphere2 s;
		s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
		//s.Mat = { 0.0f, 0.0f, 0.0f, 0.0f };
		s.Mat = 0;
		Spheres.push_back(s);
	}
	{
		Sphere2 s;
		s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
		//s.Mat = { 1.0f, 0.0f, 0.0f, 0.0f };
		s.Mat = 1;
		Spheres.push_back(s);
	}
	{
		Sphere2 s;
		s.Position = { 0.0f, -101.0f, 0.0f, 100.0f };
		//s.Mat = { 2.0f, 0.0f, 0.0f, 0.0f };
		s.Mat = 2;
		Spheres.push_back(s);
	}
}

void World::PrepareRandomScene() {
	BackgroundColor = { 0.54f, 0.73f, 0.95f };

	Sphere2 base = { {0.0f, -1001.0f, 0.0f, 1000.0f}, 3.0f };
	Sphere2 sphere1 = { {0.0f, 0.0f, 0.0f, 1.0f}, 0.0f };
	Sphere2 sphere2 = { {-3.0f, 0.0f, 0.0f, 1.0f}, 2.0f };
	Sphere2 sphere3 = { {3.0f, 0.0f, 0.0f, 1.0f}, 1.0f };

	Material dielectric, metal, diffuse, baseMat;
	dielectric = CreateDefaultDielectric();
	metal = CreateDefaultMetal();
	diffuse = CreateDefaultDiffuse();
	baseMat.Color = { 0.5f, 0.5f, 0.5f, 1.0f };
	baseMat.Roughness = 0.8f;

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
				mat = CreateRandom("material");
				Sphere2 s = { {center, 0.2f}, 0.0 };

				Materials.push_back(mat);
				Spheres.push_back(s);
			}
		}
	}

	for (int i = 4; i < Spheres.size(); i++) {
		Spheres.at(i).Mat = Random::GetFloat(3, Materials.size() - 1);
	}

}
