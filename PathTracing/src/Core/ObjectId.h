#pragma once

#ifndef CORE_OBJECTID_H
#define CORE_OBJECTID_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Lucerna {

	// Identita' stabile di un oggetto (ADR 0012): un intero casuale a 64 bit, come gli uid di
	// Godot e gli UUID di Hazel. Si crea con l'oggetto e non cambia mai, neanche se l'oggetto si
	// rinomina o si sposta nella sua lista. Con 64 bit le collisioni restano trascurabili anche
	// con milioni di oggetti. Zero vuol dire "nessuno".
	struct ObjectId {
		uint64_t Value = 0;

		static ObjectId Generate();
		// 16 cifre esadecimali; nullopt se malformato (anche un vecchio UUID a 128 bit)
		static std::optional<ObjectId> Parse(const std::string& text);
		std::string ToString() const;

		bool IsNil() const { return Value == 0; }
		bool operator==(const ObjectId&) const = default;
	};
}

template<>
struct std::hash<Lucerna::ObjectId> {
	size_t operator()(const Lucerna::ObjectId& id) const noexcept { return std::hash<uint64_t>()(id.Value); }
};

#endif // CORE_OBJECTID_H
