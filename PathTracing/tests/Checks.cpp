// Controlli della logica senza GPU: si lanciano con ./build/LucernaChecks, escono con 1 al primo
// che fallisce. Ne aggiungo uno quando una logica ha casi limite che un render non mostra.
#include "Core/ObjectId.h"
#include "Scene/SceneObject.h"

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
	return failures ? 1 : 0;
}
