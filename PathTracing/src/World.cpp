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
		case SceneType::CORNELL_BOX:
			PrepareMaterials();
			PrepareCornellBox();
			break;
        default:
            break;
    }
}

void World::DestroyScene() {
    Spheres.clear();
	Quads.clear();
    Materials.clear();
}

void World::PrepareMaterials() {
	Material redMaterial, blueMaterial, metal, lightMaterial, greenMaterial, whiteMaterial, glassMaterial;
	redMaterial.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
	redMaterial.Roughness = 0.5f;
	blueMaterial.Color = { 0.0f, 0.0f, 1.0f, 1.0f };
	blueMaterial.Roughness = 0.0f;
	metal.Color = { 0.3294f, 0.7019f, 0.6118f, 1.0f };
	metal.Roughness = 0.0f;
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
	Materials.push_back(metal);
	Materials.push_back(lightMaterial);
	Materials.push_back(whiteMaterial);
	Materials.push_back(greenMaterial);
	Materials.push_back(glassMaterial);
}

void World::PrepareSimpleScene() {
	{
		Sphere s;
		s.Position = { -0.8f, 0.0f, 0.0f, 1.0f };
		s.Mat = 2;
		Spheres.push_back(s);
	}
	{
		Sphere s;
		s.Position = { 1.0f, -0.2f, 0.0f, 0.8f };
		s.Mat = 3;
		Spheres.push_back(s);
	}
	{
		Sphere s;
		s.Position = { 0.0f, -101.0f, 0.0f, 100.0f };
		s.Mat = 0;
		Spheres.push_back(s);
	}
	// right red
	{
		Quad quad;
		quad.PositionLLC = { 2.0f, -2.0f, 0.0f, 0.0f };
		quad.U = { 0.0f, 0.0f, 1.0f, 1.0f };
		quad.V = { 0.0f, 1.0f, 0.0f, 1.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
		quad.MaterialIndex = 2;

		Quads.push_back(quad);
	}
}

void World::PrepareCornellBox() {
	{
		Sphere s;
		s.Position = { -0.9f, -1.2f, 1.5f, 0.8f };
		s.Mat = 2;
		Spheres.push_back(s);
	}
	{
		Sphere s;
		s.Position = { 0.0f, -1.6f, 2.3f, 0.4f };
		s.Mat = 2;
		Spheres.push_back(s);
	}
	{
		Sphere s;
		s.Position = { 0.9f, -1.0f, 1.1f, 1.0f };
		s.Mat = 2;
		Spheres.push_back(s);
	}
	// left green
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 4.0f, 0.0f };
		quad.U = { 0.0f, 0.0f, -1.0f, 0.0f };
		quad.V = { 0.0f, 1.0f, 0.0f, 0.0f };
		quad.Width = 4.0f;
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
		quad.Width = 4.0f;
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
		quad.Height = 4.0f;
		quad.MaterialIndex = 4;

		Quads.push_back(quad);
	}

	// bot white
	{
		Quad quad;
		quad.PositionLLC = { -2.0f, -2.0f, 4.0f, 0.0f };
		quad.U = { 1.0f, 0.0f, 0.0f, 0.0f };
		quad.V = { 0.0f, 0.0f, -1.0f, 0.0f };
		quad.Width = 4.0f;
		quad.Height = 4.0f;
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

void World::PrepareRandomScene() {
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
				Sphere s = { {center, 0.2f}, 0.0 };

				Materials.push_back(mat);
				Spheres.push_back(s);
			}
		}
	}

	for (int i = 4; i < Spheres.size(); i++) {
		Spheres.at(i).Mat = Random::GetFloat(3, Materials.size() - 1);
	}
}
