#include "Serialization/BuiltinScenes.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Lucerna {

	std::vector<BuiltinScene> ListBuiltinScenes(const std::string& directory) {
		std::vector<BuiltinScene> scenes;
		std::error_code ec;
		for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
			if (entry.path().extension() != ".json")
				continue;
			const std::string stem = entry.path().stem().string();
			const size_t dash = stem.find('-');
			if (dash == 0 || dash == std::string::npos || dash + 1 == stem.size()
			    || !std::all_of(stem.begin(), stem.begin() + dash, [](unsigned char c) { return std::isdigit(c); }))
				continue;

			BuiltinScene scene;
			scene.Number = std::stoi(stem.substr(0, dash));
			scene.Key = stem.substr(dash + 1);
			scene.Path = entry.path().string();
			// "cornell-box-meshes" -> "Cornell Box Meshes"
			bool wordStart = true;
			for (char c : scene.Key) {
				scene.DisplayName += c == '-' ? ' ' : (wordStart ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
				wordStart = c == '-';
			}
			scenes.push_back(scene);
		}
		std::sort(scenes.begin(), scenes.end(), [](const BuiltinScene& a, const BuiltinScene& b) { return a.Number < b.Number; });
		return scenes;
	}

	const BuiltinScene* FindBuiltinScene(const std::vector<BuiltinScene>& scenes, const std::string& key) {
		const bool number = !key.empty() && std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isdigit(c); });
		for (const BuiltinScene& scene : scenes)
			if (number ? scene.Number == std::stoi(key) : scene.Key == key)
				return &scene;
		return nullptr;
	}
}
