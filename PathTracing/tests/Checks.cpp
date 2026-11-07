// Controlli della logica senza GPU: si lanciano con ./build/LucernaChecks, escono con 1 al primo
// che fallisce. Ne aggiungo uno quando una logica ha casi limite che un render non mostra.
#include "Core/ObjectId.h"
#include "Core/SceneObject.h"
#include "Legacy/World.h"

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

	std::printf(failures ? "%d checks failed\n" : "all checks passed\n", failures);
	// Change tracking (issue #25): ogni modifica dice cosa ha toccato, e solo quello
	World world;
	CHECK(world.TakeChanges() == AllChanged);           // una scena nuova e' tutta da caricare
	CHECK(world.TakeChanges() == NoChange);             // e i cambiamenti si consumano
	const int m = world.AddMaterial(Material{});
	CHECK(world.TakeChanges() == MaterialsChanged);
	const int s = world.AddSphere(Sphere{});
	CHECK(world.TakeChanges() == PrimitivesChanged);
	const ObjectId sphereId = world.Spheres()[s].Id;
	Sphere moved = world.Spheres()[s];
	moved.Position.x = 3.0f;
	world.Set(sphereId, moved);
	CHECK(world.TakeChanges() == PrimitivesChanged && world.Spheres()[s].Position.x == 3.0f);
	CHECK(world.Spheres()[s].Id == sphereId);           // Set cambia i dati, non l'identita'
	world.Set(world.Materials()[m].Id, Material{});
	CHECK(world.TakeChanges() == MaterialsChanged);
	world.Rename(sphereId, "ball");                     // il nome non cambia l'immagine
	CHECK(world.TakeChanges() == NoChange && world.Spheres()[s].Name == "ball");
	world.MoveCamera(glm::vec3(1.0f), glm::vec3(0.0f, 0.0f, -1.0f)); // la vista che naviga
	CHECK(world.TakeChanges() == NoChange && world.Camera().Position == glm::vec3(1.0f));
	world.SetCamera(world.Camera());
	CHECK(world.TakeChanges() == CameraChanged);
	world.Set(ObjectId::Generate(), moved);             // un id che non c'e' non fa niente
	CHECK(world.TakeChanges() == NoChange);
	world.CreateBox(glm::vec3(0.0f), glm::vec3(1.0f), ObjectId{});
	CHECK(world.TakeChanges() == PrimitivesChanged);
	Box box = world.Boxes()[0];
	const std::vector<Quad> facesBefore = world.Quads().Pack();
	box.Max = glm::vec4(2.0f, 1.0f, 1.0f, 0.0f);
	world.Set(world.Boxes()[0].Id, box);
	CHECK(world.TakeChanges() == PrimitivesChanged);
	const std::vector<Quad> facesAfter = world.Quads().Pack();
	bool facesMoved = false;                            // il box riscrive le sue 6 facce
	for (size_t i = 0; i < facesAfter.size(); i++)
		facesMoved |= facesAfter[i].Width != facesBefore[i].Width || facesAfter[i].Height != facesBefore[i].Height
		              || facesAfter[i].PositionLLC != facesBefore[i].PositionLLC;
	CHECK(facesAfter.size() == 6 && facesMoved);
	world.RemoveSphere(s);
	CHECK(world.TakeChanges() == PrimitivesChanged && world.Spheres().empty());

	// Asset (issue #26): lo stesso OBJ piazzato due volte si carica una volta sola
	World assets;
	const int first = assets.AddMesh("res/models/pawn.obj", glm::vec3(0.0f), ObjectId{});
	const int second = assets.AddMesh("res/models/pawn.obj", glm::vec3(1.0f), ObjectId{});
	CHECK(first == 0 && second == 1 && assets.Meshes().size() == 2);
	CHECK(assets.Assets().Meshes.size() == 1 && assets.Meshes()[0].Asset == assets.Meshes()[1].Asset);
	CHECK(assets.AssetOf(assets.Meshes()[1]) != nullptr && !assets.AssetOf(assets.Meshes()[1])->Triangles.empty());

	// Una texture usata da un materiale non si toglie; una inutilizzata si', e libera il suo posto
	assets.TakeChanges();
	assets.AddTexture("textures/a.png", TextureKind::Color);
	assets.AddTexture("textures/a.png", TextureKind::Color);          // stessa immagine e tipo: una sola
	assets.AddTexture("textures/a.png", TextureKind::Data);           // come dato e' un altro asset
	CHECK(assets.Textures().size() == 2 && assets.TakeChanges() == TexturesChanged);
	Material textured;
	textured.AlbedoTexture = assets.Textures()[0].Id;
	assets.AddMaterial(textured);
	CHECK(!assets.RemoveTexture(assets.Textures()[0].Id) && assets.Textures().size() == 2);
	CHECK(assets.RemoveTexture(assets.Textures()[1].Id) && assets.Textures().size() == 1);

	return failures ? 1 : 0;
}
