// Controlli della logica senza GPU: si lanciano con ./build/LucernaChecks, escono con 1 al primo
// che fallisce. Ne aggiungo uno quando una logica ha casi limite che un render non mostra.
#include "Core/ObjectId.h"
#include "Core/SceneObject.h"
#include "Scene/Scene.h"
#include "Import/SceneFiles.h"
#include "Import/Importer.h"
#include "Serialization/BuiltinScenes.h"

#include <cstdio>
#include <unordered_set>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

struct Thing { int Value = 0; };

int main() {
	using namespace Lucerna;

	// ObjectId: 16 cifre esadecimali che si rileggono uguali; zero e' "nessuno"
	const ObjectId id = ObjectId::Generate();
	const std::string text = id.ToString();
	CHECK(text.size() == 16 && !id.IsNil());
	CHECK(ObjectId::Parse(text) == id);
	CHECK(!ObjectId::Parse("not-an-id").has_value());
	CHECK(!ObjectId::Parse("3f2a9c1e-7b4d-4e8a-9c2f-1a2b3c4d5e6f").has_value()); // vecchio UUID a 128 bit
	CHECK(ObjectId::Parse("0000000000000000")->IsNil());
	CHECK(ObjectId::Parse("00000000000000ff")->Value == 255);
	std::unordered_set<ObjectId> seen;
	for (int i = 0; i < 10000; i++) seen.insert(ObjectId::Generate());
	CHECK(seen.size() == 10000);

	// Nomi unici come in Blender
	ObjectList<Thing> list("Thing");
	CHECK(list.Add({}).Name == "Thing");
	CHECK(list.Add({}).Name == "Thing.001");
	CHECK(list.Add({}, "stone").Name == "stone");
	CHECK(list.Add({}, "stone").Name == "stone.001");
	CHECK(list.Add({}, "stone.001").Name == "stone.002");   // la base e' "stone"
	CHECK(list.Add({}, "v1.5").Name == "v1.5");             // ".5" non e' un suffisso .NNN
	list.Rename(list[2], "stone");                            // rinominarsi col proprio nome non lo cambia
	CHECK(list[2].Name == "stone");
	list.Rename(list[2], "Thing");
	CHECK(list[2].Name == "Thing.002");

	// Identita' stabile: cancellare un oggetto non cambia gli altri, e assegnare dati la conserva
	const ObjectId third = list[3].Id;
	list.erase(list.begin() + 1);
	CHECK(list.IndexOf(third) == 2 && list[2].Name == "stone.001");
	list[2] = Thing{ 42 };
	CHECK(list[2].Id == third && list[2].Value == 42 && list[2].Name == "stone.001");
	CHECK(list.Pack().size() == list.size());

	// Change tracking (issue #25, #29): ogni modifica dice cosa ha toccato, e solo quello.
	// Scene e AssetLibrary hanno ciascuna i suoi flag.
	Scene scene;
	AssetLibrary library;
	CHECK(scene.TakeChanges() == SceneChanges && library.TakeChanges() == AssetChanges); // tutto da caricare
	CHECK(scene.TakeChanges() == NoChange && library.TakeChanges() == NoChange);         // e si consumano
	const ObjectId materialId = library.AddMaterial(Material{});
	CHECK(library.TakeChanges() == MaterialsChanged && scene.TakeChanges() == NoChange);
	const int s = scene.AddSphere(Sphere{});
	CHECK(scene.TakeChanges() == PrimitivesChanged);
	const ObjectId sphereId = scene.Spheres()[s].Id;
	Sphere moved = scene.Spheres()[s];
	moved.Position.x = 3.0f;
	scene.Set(sphereId, moved);
	CHECK(scene.TakeChanges() == PrimitivesChanged && scene.Spheres()[s].Position.x == 3.0f);
	CHECK(scene.Spheres()[s].Id == sphereId);           // Set cambia i dati, non l'identita'
	library.Set(materialId, Material{});
	CHECK(library.TakeChanges() == MaterialsChanged);
	CHECK(scene.Rename(sphereId, "ball") && !library.Rename(sphereId, "ball")); // il nome non cambia l'immagine
	CHECK(scene.TakeChanges() == NoChange && scene.Spheres()[s].Name == "ball");
	CHECK(library.Rename(materialId, "clay") && library.TakeChanges() == NoChange);
	scene.MoveCamera(glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)); // la vista che naviga
	CHECK(scene.TakeChanges() == NoChange && scene.Camera().Position == glm::vec3(1.0f));
	scene.SetCamera(scene.Camera());
	CHECK(scene.TakeChanges() == CameraChanged);
	scene.Set(ObjectId::Generate(), moved);             // un id che non c'e' non fa niente
	CHECK(scene.TakeChanges() == NoChange);
	scene.CreateBox(glm::vec3(0.0f), glm::vec3(1.0f), ObjectId{});
	CHECK(scene.TakeChanges() == PrimitivesChanged);
	Box box = scene.Boxes()[0];
	const std::vector<Quad> facesBefore = scene.Quads().Pack();
	box.Max = glm::vec4(2.0f, 1.0f, 1.0f, 0.0f);
	scene.Set(scene.Boxes()[0].Id, box);
	CHECK(scene.TakeChanges() == PrimitivesChanged);
	const std::vector<Quad> facesAfter = scene.Quads().Pack();
	bool facesMoved = false;                            // il box riscrive le sue 6 facce
	for (size_t i = 0; i < facesAfter.size(); i++)
		facesMoved |= facesAfter[i].Width != facesBefore[i].Width || facesAfter[i].Height != facesBefore[i].Height
		              || facesAfter[i].PositionLLC != facesBefore[i].PositionLLC;
	CHECK(facesAfter.size() == 6 && facesMoved);
	scene.RemoveSphere(s);
	CHECK(scene.TakeChanges() == PrimitivesChanged && scene.Spheres().empty());

	// Asset (issue #26): lo stesso OBJ piazzato due volte si carica una volta sola
	Scene placed;
	AssetLibrary assets;
	const int first = AddFile("res/models/pawn.obj", placed, assets, glm::mat4(1.0f), ObjectId{});
	const int second = AddFile("res/models/pawn.obj", placed, assets, glm::mat4(2.0f), ObjectId{});
	CHECK(first == 0 && second == 1 && placed.Meshes().size() == 2);
	CHECK(assets.Meshes().size() == 1 && placed.Meshes()[0].Asset == placed.Meshes()[1].Asset);
	CHECK(assets.FindMesh(placed.Meshes()[1].Asset) != nullptr && !assets.FindMesh(placed.Meshes()[1].Asset)->Triangles.empty());

	// Una texture usata da un materiale non si toglie; una inutilizzata si', e libera il suo posto
	assets.TakeChanges();
	assets.RegisterTexture("textures/a.png", TextureKind::Color);
	assets.RegisterTexture("textures/a.png", TextureKind::Color);     // stessa immagine e tipo: una sola
	assets.RegisterTexture("textures/a.png", TextureKind::Data);      // come dato e' un altro asset
	CHECK(assets.Textures().size() == 2 && assets.TakeChanges() == TexturesChanged);
	Material textured;
	textured.AlbedoTexture = assets.Textures()[0].Id;
	assets.AddMaterial(textured);
	CHECK(!assets.RemoveTexture(assets.Textures()[0].Id) && assets.Textures().size() == 2);
	CHECK(assets.RemoveTexture(assets.Textures()[1].Id) && assets.Textures().size() == 1);

	// Registry degli importer (issue #27): per estensione, senza badare alle maiuscole
	CHECK(FindImporter("a/b/model.OBJ") != nullptr && std::string(FindImporter("a/b/model.OBJ")->Name()) == "Wavefront OBJ");
	CHECK(FindImporter("scene.gltf") != nullptr && FindImporter("scene.glb") == FindImporter("scene.gltf"));
	CHECK(FindImporter("scene.fbx") == nullptr && FindImporter("noextension") == nullptr);
	// Un file che non si apre lascia scena e asset come erano
	const size_t instancesBefore = placed.Meshes().size(), materialsBefore = assets.Materials().size();
	CHECK(!OpenFile("res/models/missing.gltf", placed, assets) && !OpenFile("res/models/scene.fbx", placed, assets));
	CHECK(placed.Meshes().size() == instancesBefore && assets.Materials().size() == materialsBefore);
	CHECK(AddFile("res/models/missing.obj", placed, assets, glm::mat4(1.0f), ObjectId{}) == -1);
	// Open di un OBJ: la scena diventa il modello, con i suoi asset e un materiale di default
	CHECK(OpenFile("res/models/pawn.obj", placed, assets) && placed.Meshes().size() == 1 && assets.Meshes().size() == 1);
	CHECK(assets.Materials().size() == 1 && placed.Meshes()[0].Material == assets.Materials()[0].Id);

	// Scene integrate (issue #28): i file di res/scenes/builtin, per numero o per nome
	const std::vector<BuiltinScene> builtin = ListBuiltinScenes("res/scenes/builtin");
	CHECK(builtin.size() == 7 && builtin.front().Number == 0 && builtin.back().Number == 8);
	CHECK(FindBuiltinScene(builtin, "6") != nullptr && FindBuiltinScene(builtin, "6")->Key == "setup-2");
	CHECK(FindBuiltinScene(builtin, "sponza") == &builtin.back());
	CHECK(FindBuiltinScene(builtin, "4") != nullptr && FindBuiltinScene(builtin, "4")->DisplayName == "Cornell Box Meshes");
	CHECK(FindBuiltinScene(builtin, "1") == nullptr && FindBuiltinScene(builtin, "") == nullptr);  // le scene casuali non ci sono piu'

	std::printf(failures ? "%d checks failed\n" : "all checks passed\n", failures);
	return failures ? 1 : 0;
}
