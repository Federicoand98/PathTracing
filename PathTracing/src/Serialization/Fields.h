#pragma once

#ifndef SERIALIZATION_FIELDS_H
#define SERIALIZATION_FIELDS_H

#include <tuple>

/*
* Tabelle di campi per la serializzazione (ADR 0010).
*
* Una struct diventa serializzabile dichiarando, accanto a se', una funzione libera
* JsonFields che elenca i suoi campi con la chiave JSON di ciascuno:
*
*     struct Foo { int Count = 1; float Scale = 2.0f; };
*     constexpr auto JsonFields(const Foo*) {
*         return std::make_tuple(Field("count", &Foo::Count), Field("scale", &Foo::Scale));
*     }
*
* Basta questo: Json.h le da' to_json/from_json, quindi si scrive `json j = foo;` e si legge
* `j.get_to(foo)`. Le chiavi sono esplicite, non i nomi dei membri: rinominare un membro non
* rompe i file gia' salvati. In lettura una chiave assente lascia il valore che il campo
* aveva, cioe' il default della struct, e le chiavi sconosciute si ignorano.
*
* Un campo che nel file non coincide col membro (un enum scritto come stringa, un bool che
* diventa "point"/"spot") usa Custom, con una funzione che legge e una che scrive:
*
*     Custom<Light>("type", [](const Light& l) { return l.Spot ? "spot" : "point"; },
*                           [](Light& l, const std::string& s) { l.Spot = s == "spot"; })
*
* Questo header non dipende dalla libreria JSON: le struct lo includono senza tirarsela dietro.
*/

namespace PathTracer {

	namespace Reflect {
		template<class T, class M>
		struct FieldRef {
			const char* Key;
			M T::* Member;
		};

		template<class T, class Get, class Set>
		struct CustomRef {
			const char* Key;
			Get Read;   // const T& -> valore da scrivere nel file
			Set Write;  // (T&, valore letto dal file)
		};
	}

	template<class T, class M>
	constexpr Reflect::FieldRef<T, M> Field(const char* key, M T::* member) { return { key, member }; }

	template<class T, class Get, class Set>
	constexpr Reflect::CustomRef<T, Get, Set> Custom(const char* key, Get read, Set write) { return { key, read, write }; }
}

#endif // SERIALIZATION_FIELDS_H
