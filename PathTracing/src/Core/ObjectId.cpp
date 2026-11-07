#include "Core/ObjectId.h"

#include <random>

namespace Lucerna {

	// Un generatore suo, seminato una volta dal sistema: non tocca la sequenza di Random, da cui
	// dipendono le scene casuali.
	ObjectId ObjectId::Generate() {
		static std::mt19937_64 engine{ (static_cast<uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}() };
		ObjectId id;
		do { id.Value = engine(); } while (id.Value == 0); // zero e' riservato a "nessuno"
		return id;
	}

	std::string ObjectId::ToString() const {
		char text[17];
		snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(Value));
		return text;
	}

	std::optional<ObjectId> ObjectId::Parse(const std::string& text) {
		if (text.size() != 16)
			return std::nullopt;
		ObjectId id;
		for (char c : text) {
			int v;
			if (c >= '0' && c <= '9') v = c - '0';
			else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
			else return std::nullopt;
			id.Value = (id.Value << 4) | static_cast<uint64_t>(v);
		}
		return id;
	}
}
