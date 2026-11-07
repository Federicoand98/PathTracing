#include "Import/Importer.h"

#include <algorithm>
#include <cctype>

namespace Lucerna {

	std::unique_ptr<Importer> MakeObjImporter();
	std::unique_ptr<Importer> MakeGltfImporter();

	const std::vector<std::unique_ptr<Importer>>& Importers() {
		static const std::vector<std::unique_ptr<Importer>> importers = [] {
			std::vector<std::unique_ptr<Importer>> list;
			list.push_back(MakeObjImporter());
			list.push_back(MakeGltfImporter());
			return list;
		}();
		return importers;
	}

	const Importer* FindImporter(const std::string& path) {
		std::string extension = std::filesystem::path(path).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(),
		               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		for (const auto& importer : Importers())
			for (const std::string& e : importer->Extensions())
				if (e == extension)
					return importer.get();
		return nullptr;
	}
}
