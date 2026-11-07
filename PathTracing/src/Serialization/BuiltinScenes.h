#pragma once

#ifndef SERIALIZATION_BUILTINSCENES_H
#define SERIALIZATION_BUILTINSCENES_H

#include <string>
#include <vector>

namespace Lucerna {

	// Le scene integrate (issue #28) sono file di scena in res/scenes/builtin, uno per scena:
	// aggiungerne una vuol dire salvarne il file li'. Il nome del file da' numero e nome,
	// "06-setup-2.json" = scena 6, "setup-2", che nel combo diventa "Setup 2".
	struct BuiltinScene {
		int Number = -1;
		std::string Key;          // "setup-2"
		std::string DisplayName;  // "Setup 2"
		std::string Path;
	};

	// Ordinate per numero. Un file senza il prefisso "NN-" si ignora.
	std::vector<BuiltinScene> ListBuiltinScenes(const std::string& directory = "scenes/builtin");

	// Per numero ("6") o per nome ("setup-2"); nullptr se non c'e'
	const BuiltinScene* FindBuiltinScene(const std::vector<BuiltinScene>& scenes, const std::string& key);
}

#endif // SERIALIZATION_BUILTINSCENES_H
