#pragma once

#ifndef SCENE_SCENEOBJECT_H
#define SCENE_SCENEOBJECT_H

#include "Core/ObjectId.h"

#include <string>
#include <utility>
#include <vector>

namespace Lucerna {

	// Un oggetto della scena (issue #23): i suoi dati, che sono anche la struct GPU (Sphere,
	// Quad, Material...), piu' l'identita'. Eredita dai dati, quindi sphere.Position si legge
	// come prima; l'identita' non entra nella struct, che resta il layout std430.
	template<class T>
	struct SceneObject : T {
		ObjectId Id;
		std::string Name;

		SceneObject() = default;
		SceneObject(const T& data, ObjectId id, std::string name) : T(data), Id(id), Name(std::move(name)) {}

		// Assegnare i dati non tocca l'identita': l'oggetto resta lo stesso, cambia com'e' fatto
		SceneObject& operator=(const T& data) { T::operator=(data); return *this; }
	};

	// La lista di un tipo di oggetti: aggiunge con un id nuovo e un nome unico, trova per id,
	// e impacchetta i soli dati per la GPU. Si legge come un std::vector (indice, size, range-for);
	// si aggiunge solo con Add, cosi' nessun oggetto resta senza identita'.
	//
	// I nomi sono unici nella lista, come i data-block di Blender: un secondo "stone" diventa
	// "stone.001". L'identita' resta comunque l'id.
	template<class T>
	class ObjectList {
	public:
		using Object = SceneObject<T>;

		explicit ObjectList(std::string defaultName) : m_DefaultName(std::move(defaultName)) {}

		// id nullo = uno nuovo; nome vuoto = quello di default del tipo
		Object& Add(const T& data, const std::string& name = {}, ObjectId id = {}) {
			m_Items.emplace_back(data, id.IsNil() ? ObjectId::Generate() : id, UniqueName(name.empty() ? m_DefaultName : name, nullptr));
			return m_Items.back();
		}

		void Rename(Object& object, const std::string& name) {
			object.Name = UniqueName(name.empty() ? m_DefaultName : name, &object);
		}

		int IndexOf(const ObjectId& id) const {
			for (size_t i = 0; i < m_Items.size(); i++)
				if (m_Items[i].Id == id)
					return static_cast<int>(i);
			return -1;
		}
		Object* Find(const ObjectId& id) {
			const int i = IndexOf(id);
			return i >= 0 ? &m_Items[i] : nullptr;
		}

		// I soli dati, contigui: quello che va nei buffer della GPU.
		std::vector<T> Pack() const { return std::vector<T>(m_Items.begin(), m_Items.end()); }

		// ---- accesso come un vettore
		size_t size() const { return m_Items.size(); }
		bool empty() const { return m_Items.empty(); }
		Object& operator[](size_t i) { return m_Items[i]; }
		const Object& operator[](size_t i) const { return m_Items[i]; }
		Object& at(size_t i) { return m_Items.at(i); }
		const Object& at(size_t i) const { return m_Items.at(i); }
		Object& back() { return m_Items.back(); }
		auto begin() { return m_Items.begin(); }
		auto end() { return m_Items.end(); }
		auto begin() const { return m_Items.begin(); }
		auto end() const { return m_Items.end(); }
		void clear() { m_Items.clear(); }
		auto erase(typename std::vector<Object>::const_iterator first, typename std::vector<Object>::const_iterator last) { return m_Items.erase(first, last); }
		auto erase(typename std::vector<Object>::const_iterator it) { return m_Items.erase(it); }

		const std::string& DefaultName() const { return m_DefaultName; }

	private:
		// Il nome richiesto se e' libero, altrimenti la sua base (senza un ".NNN" finale) con il
		// primo suffisso libero. except = l'oggetto che si sta rinominando, che non conta.
		std::string UniqueName(const std::string& wanted, const Object* except) const {
			auto taken = [&](const std::string& n) {
				for (const Object& o : m_Items)
					if (&o != except && o.Name == n)
						return true;
				return false;
			};
			if (!taken(wanted))
				return wanted;
			std::string base = wanted;
			const size_t dot = base.rfind('.');
			if (dot != std::string::npos && dot + 4 == base.size()
			    && base.find_first_not_of("0123456789", dot + 1) == std::string::npos)
				base.resize(dot);
			for (int n = 1;; n++) {
				char suffix[16];
				snprintf(suffix, sizeof(suffix), ".%03d", n);
				if (!taken(base + suffix))
					return base + suffix;
			}
		}

		std::vector<Object> m_Items;
		std::string m_DefaultName;
	};
}

#endif // SCENE_SCENEOBJECT_H
