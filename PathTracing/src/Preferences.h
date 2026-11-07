#pragma once

#ifndef PREFERENCES_H
#define PREFERENCES_H

#include "Serialization/Fields.h"

namespace PathTracer {

	// Preferenze dell'applicazione (ADR 0010): valgono per ogni scena e restano tra un avvio e
	// l'altro, in preferences.json accanto a imgui.ini.
	struct Preferences {
		bool Vsync = true;

		// File mancante o illeggibile: i default, senza errore.
		static Preferences Load();
		void Save() const;
	};

	constexpr auto JsonFields(const Preferences*) {
		return std::make_tuple(Field("vsync", &Preferences::Vsync));
	}
}

#endif // PREFERENCES_H
