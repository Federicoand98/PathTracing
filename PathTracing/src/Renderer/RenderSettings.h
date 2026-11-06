#pragma once

#ifndef RENDERSETTINGS_H
#define RENDERSETTINGS_H

#include "../Serialization/Fields.h"

namespace PathTracer {

	// Come si calcola l'immagine (ADR 0010): stessa scena, qualita' diversa. Si salva con la
	// scena, cosi' una scena preparata per il render finale si riapre con la sua qualita'.
	// I default sono il preset Preview.
	struct RenderSettings {
		// ---- integratore
		int SamplesPerPixel = 1;
		int RayDepth = 5;
		// Next Event Estimation (light sampling + MIS): a ogni vertice non-speculare campiona
		// direttamente gli Emitter invece di aspettare che un rimbalzo casuale li colpisca.
		// Lato shader e' numLights: qui lo si azzera per spegnerla senza toccare la light list.
		bool NEE = true;
		float FireflyClamp = 0.0f;   // luminanza massima per campione, 0 = off

		// ---- accumulazione
		// Riproiezione temporale (ADR 0002): al posto di azzerare l'accumulo quando la camera
		// si muove, si riusa il frame precedente riproiettandolo. Il cap limita il ghosting da
		// storia stantia e vale solo in movimento; da fermo N cresce libero e converge.
		bool Reproject = true;
		int HistoryCap = 32;
		float RenderScale = 1.0f;    // 1 = piena risoluzione; < 1 = ridotta mentre ci si muove

		// ---- denoiser à-trous (ADR 0001): filtro di sola vista, agisce sulla vista Beauty
		bool Denoise = false;
		float DenoiseStrength = 1.0f; // [0,1] peso del blend col beauty grezzo (1 = tutto denoised)
		// Auto-attenuazione: scala lo strength ~1/sqrt(N) col numero di campioni. A immagine
		// convergente il denoiser si spegne; resta forte solo sui frame rumorosi.
		bool DenoiseAutoFade = true;
		float DenoiseCPhi = 1.0f;    // sigma luminanza (grande = piu' sfoca ombre/rumore)
		float DenoiseNPhi = 64.0f;   // esponente normale (grande = spigoli piu' netti)
		float DenoisePPhi = 1.0f;    // sigma profondita' in unita' mondo (separa le silhouette)
		float DenoiseAPhi = 0.2f;    // sigma albedo (piccolo = blocca i bordi di materiale/tinta)

		// ---- film
		bool Tonemap = true;         // ACES; off = valori lineari clampati
		float Exposure = 1.0f;

		// Preset: punti di partenza, non modalita'. Cambiare un campo dopo averne scelto uno
		// e' normale.
		static RenderSettings Draft() {
			RenderSettings s;
			s.RayDepth = 3;
			s.RenderScale = 0.5f;
			s.Denoise = true;
			return s;
		}
		static RenderSettings Preview() { return {}; }
		static RenderSettings Final() {
			RenderSettings s;
			s.SamplesPerPixel = 4;
			s.RayDepth = 12;
			return s;
		}
	};

	constexpr auto JsonFields(const RenderSettings*) {
		using S = RenderSettings;
		return std::make_tuple(
			Field("samplesPerPixel", &S::SamplesPerPixel), Field("rayDepth", &S::RayDepth),
			Field("nee", &S::NEE), Field("fireflyClamp", &S::FireflyClamp),
			Field("reproject", &S::Reproject), Field("historyCap", &S::HistoryCap), Field("renderScale", &S::RenderScale),
			Field("denoise", &S::Denoise), Field("denoiseStrength", &S::DenoiseStrength), Field("denoiseAutoFade", &S::DenoiseAutoFade),
			Field("denoiseCPhi", &S::DenoiseCPhi), Field("denoiseNPhi", &S::DenoiseNPhi),
			Field("denoisePPhi", &S::DenoisePPhi), Field("denoiseAPhi", &S::DenoiseAPhi),
			Field("tonemap", &S::Tonemap), Field("exposure", &S::Exposure));
	}

	// Cosa si sta guardando: non cambia mai l'immagine calcolata e non si salva.
	struct ViewSettings {
		int AOVView = 0;             // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth, 4 = storia
		bool PathTracing = true;     // false = niente accumulazione: ogni frame riparte da un campione
		// true = la vista guarda attraverso la camera della scena e navigando la sposta;
		// false = vista libera, l'inquadratura resta dov'e' (ADR 0011)
		bool LockViewToCamera = true;
	};
}

#endif // RENDERSETTINGS_H
