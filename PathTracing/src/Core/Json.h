#pragma once

#ifndef SERIALIZATION_JSON_H
#define SERIALIZATION_JSON_H

#include "Core/Fields.h"

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <type_traits>
#include <utility>

/*
* to_json/from_json per qualunque struct con una tabella JsonFields (vedi Fields.h e
* ADR 0010), piu' i tipi glm. nlohmann trova queste funzioni da solo, per ADL: una volta
* incluso questo header, `json j = settings;` e `j.get_to(settings)` funzionano, anche per
* struct annidate e std::vector di struct.
*/

// ---- glm: vettori e matrici come array di float ---------------------------
// In lettura un valore che non e' un array della lunghezza giusta lascia il campo com'era,
// come facevano i lettori scritti a mano: un file ritoccato male non fa fallire il load.

namespace nlohmann {
	template<glm::length_t L, glm::qualifier Q>
	struct adl_serializer<glm::vec<L, float, Q>> {
		static void to_json(json& j, const glm::vec<L, float, Q>& v) {
			j = json::array();
			for (glm::length_t i = 0; i < L; i++) j.push_back(v[i]);
		}
		static void from_json(const json& j, glm::vec<L, float, Q>& v) {
			if (!j.is_array() || j.size() < static_cast<size_t>(L)) return;
			for (glm::length_t i = 0; i < L; i++) v[i] = j[i].get<float>();
		}
	};

	template<glm::qualifier Q>
	struct adl_serializer<glm::mat<4, 4, float, Q>> {
		static void to_json(json& j, const glm::mat<4, 4, float, Q>& m) {
			const float* p = glm::value_ptr(m);
			j = json::array();
			for (int i = 0; i < 16; i++) j.push_back(p[i]);
		}
		static void from_json(const json& j, glm::mat<4, 4, float, Q>& m) {
			if (!j.is_array() || j.size() < 16) return;
			float* p = glm::value_ptr(m);
			for (int i = 0; i < 16; i++) p[i] = j[i].get<float>();
		}
	};
}

namespace Lucerna {

	// Una struct e' "riflessa" se esiste JsonFields(const T*): la si cerca per ADL.
	template<class T>
	concept Reflected = requires(const T* p) { JsonFields(p); };

	namespace Reflect {
		template<class T, class M>
		void Write(nlohmann::json& j, const T& v, const FieldRef<T, M>& f) { j[f.Key] = v.*f.Member; }

		template<class T, class M>
		void Read(const nlohmann::json& j, T& v, const FieldRef<T, M>& f) {
			if (auto it = j.find(f.Key); it != j.end())
				it->get_to(v.*f.Member);
		}

		template<class T, class G, class S>
		void Write(nlohmann::json& j, const T& v, const CustomRef<T, G, S>& f) { j[f.Key] = f.Read(v); }

		// Il tipo che Custom scrive nel file (quello che Read restituisce) e' anche quello letto.
		template<class T, class G, class S>
		void Read(const nlohmann::json& j, T& v, const CustomRef<T, G, S>& f) {
			using Value = std::decay_t<decltype(f.Read(std::declval<const T&>()))>;
			if (auto it = j.find(f.Key); it != j.end())
				f.Write(v, it->template get<Value>());
		}
	}

	template<Reflected T>
	void to_json(nlohmann::json& j, const T& v) {
		j = nlohmann::json::object();
		std::apply([&](const auto&... f) { (Reflect::Write(j, v, f), ...); }, JsonFields(static_cast<const T*>(nullptr)));
	}

	// Parte dal valore che v ha gia': i campi assenti dal file restano come sono.
	template<Reflected T>
	void from_json(const nlohmann::json& j, T& v) {
		if (!j.is_object()) return;
		std::apply([&](const auto&... f) { (Reflect::Read(j, v, f), ...); }, JsonFields(static_cast<const T*>(nullptr)));
	}
}

#endif // SERIALIZATION_JSON_H
