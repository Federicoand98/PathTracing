// AOV DETERMINISTICI: un raggio primario NON jitterato, tracciato una volta sola qui fuori.
// Devono essere identici a ogni frame, altrimenti:
//  - la rimodulazione (beauty = irr * albedo) oscillerebbe per sempre col jitter;
//  - la validazione della riproiezione confronta normale/depth col frame precedente: se
//    lampeggiano, i pixel di bordo vengono rifiutati SEMPRE, restano a N=1 e non fanno mai
//    anti-aliasing -> contorni aliasati e scuri, permanenti.
// In piu' cosi' non vivono dentro PerPixel: 7 float in meno nel loop dei rimbalzi, che e'
// il kernel piu' caldo del programma (l'occupancy li' vale ~1.6x di frame rate).
// Dipendendo solo dalla camera, a camera ferma sono IDENTICI a quelli del frame scorso:
// si rileggono invece di ritracciare. Due imageLoad al posto di una traversata BVH completa,
// che sul raggio primario e' la piu' cara del programma (su Sponza vale ~4x di frame rate).
// Vengono calcolati in uno scope che si CHIUDE prima del loop dei sample, e piu' sotto si
// rileggono dalle immagini. Sembra uno spreco ed e' invece il punto: se restassero vivi
// attraverso il loop, il compilatore terrebbe 7 float occupati per tutta la traversata BVH
// dei rimbalzi e l'occupancy crolla (misurato: ~1.6x di frame rate). Il giro store->load
// spezza la catena di liveness e lascia morire i registri.
void UpdateAOVs(ivec2 texelCoord, ivec2 imageResultSize) {
    vec3 a = vec3(1.0);   // default = sfondo: albedo neutro lascia la demodulazione a 1
    vec3 n = vec3(0.0);
    float d = 1e4;
    // Nell'alpha di imgAlbedo: "history trust" della superficie [0,1]. Le superfici speculari
    // (roughness bassa) hanno shading VIEW-DEPENDENT: la storia riproiettata da un'altra
    // posizione camera e' sbagliata e in movimento lascia aloni. trust ~0 le fa riaggiornare
    // in fretta (poca storia), trust ~1 lascia accumulare le superfici diffuse. Vedi la
    // riproiezione piu' sotto. Lo sfondo (default) = 1: dipende solo dalla rotazione.
    float trust = 1.0;
    if(aovNeedsUpdate == 1) {
        vec2 ndcCenter = (vec2(texelCoord) + 0.5) / vec2(imageResultSize) * 2.0 - 1.0;
        Ray aovRay = CalculateRayFromCamera(inverseProjection, inverseView, cameraPosition, ndcCenter);
        HitInfo aovHit = TraceRay(aovRay, 0.0);
        if(aovHit.HitDistance > 0.0) {
            Material aovMat = Materials[clamp(aovHit.MaterialIndex, 0, numMaterials - 1)];
            vec3 aovPos = aovRay.Origin + aovRay.Direction * aovHit.HitDistance;
            a = vec3(aovMat.Color) * SampleAlbedo(aovMat, aovHit.UV, aovPos, 0.0);
            n = aovHit.Normal;
            d = aovHit.HitDistance;
            // liscio (specchio/vetro/metallo) -> view-dependent -> trust basso.
            // ruvido (>0.5) -> diffuso -> trust pieno.
            trust = smoothstep(0.0, 0.5, aovMat.Roughness);
        }
    } else {
        // imgAlbedo non e' ping-pong: contiene ancora il frame scorso (rgb + trust in alpha).
        // NormalDepth si legge dalla storia: l'indice corrente e' il bersaglio di scrittura.
        vec4 av = imageLoad(imgAlbedo, texelCoord);
        a = av.rgb;
        trust = av.a;
        vec4 nd = imageLoad(imgNormalDepthPrev, texelCoord);
        n = nd.rgb;
        d = nd.a;
    }
    imageStore(imgAlbedo, texelCoord, vec4(a, trust));
    imageStore(imgNormalDepth, texelCoord, vec4(n, d));
}

// Valida un singolo tap della storia riproiettata e ne restituisce irradianza (irr) e sample
// count (n). Ritorna false se il tap e' fuori schermo, cade su un'altra superficie (depth o
// normale non combaciano) o contiene valori non finiti. worldPos serve solo per le superfici
// (isBg == false); per lo sfondo basta che il tap fosse sfondo anche prima.
bool ValidateHistoryTap(ivec2 pc, ivec2 size, bool isBg, vec3 worldPos, vec3 aovNormal, out vec3 irr, out float n) {
    irr = vec3(0.0);
    n = 0.0;
    if(any(lessThan(pc, ivec2(0))) || any(greaterThanEqual(pc, size)))
        return false;

    vec4 ndPrev = imageLoad(imgNormalDepthPrev, pc);

    bool ok;
    if(isBg) {
        ok = dot(ndPrev.rgb, ndPrev.rgb) < 1e-6; // era sfondo anche prima
    } else {
        // Il depth in storia e' misurato dalla camera di allora: si confronta con la distanza
        // attesa da quella posizione, non col depth corrente. Tolleranza relativa.
        float expectedPrev = length(worldPos - prevCameraPosition);
        bool depthOk = abs(ndPrev.a - expectedPrev) < 0.05 * max(expectedPrev, 1.0);
        bool normalOk = dot(normalize(ndPrev.rgb), normalize(aovNormal)) > 0.9;
        ok = depthOk && normalOk;
    }
    if(!ok)
        return false;

    vec4 h = imageLoad(imgIrradiancePrev, pc);
    if(any(isnan(h.rgb)) || any(isinf(h.rgb))) // storia non finita: va scartata o si propaga
        return false;

    irr = h.rgb;
    n = h.a;
    return true;
}

// Riproiezione temporale (ADR 0002). Invece di azzerare l'accumulo quando la camera si muove,
// si ritrova questo pixel nel frame precedente e si riusa la sua irradianza. Ritorna true se
// la storia e' valida, e in tal caso histIrr/histN ne contengono il valore.
bool ReprojectHistory(ivec2 texelCoord, ivec2 imageResultSize, vec3 aovNormal, float aovDepth, out vec3 histIrr, out float histN) {
    histIrr = vec3(0.0);
    histN = 0.0;
    bool valid = false;

    // Lo sfondo ha normale nulla. Non va escluso dalla storia: escluderlo lo inchioda a N=1
    // e non accumula MAI (grana fissa nel cielo). E' semplicemente all'infinito, quindi la
    // sua riproiezione dipende solo dalla rotazione della camera e si valida diversamente.
    bool isBackground = dot(aovNormal, aovNormal) < 1e-6;
    if(hasHistory == 1 && cameraMoved == 0) {
        // Camera ferma: la riproiezione e' l'identita' e questo pixel inquadra la STESSA
        // superficie del frame scorso, quindi la sua storia e' sempre valida: si legge diretta,
        // senza validare depth/normale (il tap e' se stessi). Solo guardia sui valori non finiti.
        // Cosi' l'accumulo converge esattamente come prima della riproiezione.
        vec4 h = imageLoad(imgIrradiancePrev, texelCoord);
        if(!any(isnan(h.rgb)) && !any(isinf(h.rgb))) {
            histIrr = h.rgb;
            histN = h.a;
            valid = true;
        }
    } else if(hasHistory == 1) {
        // In MOVIMENTO: si ritrova il pixel nel frame precedente e si legge la storia in
        // BILINEARE (2x2). Il nearest-neighbor qui creava una griglia che shimmerava (i pattern
        // di proiezione): l'arrotondamento intero di uvPrev salta di pixel a ogni frame.
        vec2 ndcCenter = (vec2(texelCoord) + 0.5) / vec2(imageResultSize) * 2.0 - 1.0;
        Ray centerRay = CalculateRayFromCamera(inverseProjection, inverseView, cameraPosition, ndcCenter);
        vec3 worldPos = centerRay.Origin + centerRay.Direction * aovDepth;

        vec4 clipPrev = prevViewProjection * vec4(worldPos, 1.0);
        if(clipPrev.w > 0.0) {  // w <= 0 = dietro la camera precedente
            vec2 uvPrev = (clipPrev.xy / clipPrev.w) * 0.5 + 0.5;
            // in spazio pixel-centro: il tap intero e' floor, i pesi vengono dalla frazione
            vec2 uvPix = uvPrev * vec2(imageResultSize) - 0.5;
            ivec2 base = ivec2(floor(uvPix));
            vec2 f = uvPix - vec2(base);
            float w[4] = float[4]((1.0-f.x)*(1.0-f.y), f.x*(1.0-f.y), (1.0-f.x)*f.y, f.x*f.y);
            ivec2 off[4] = ivec2[4](ivec2(0,0), ivec2(1,0), ivec2(0,1), ivec2(1,1));

            vec3 irrSum = vec3(0.0);
            float nSum = 0.0, wSum = 0.0;
            for(int k = 0; k < 4; k++) {
                vec3 ti; float tn;
                // ogni tap va validato per conto suo: sui bordi geometrici alcuni cadono su
                // un'altra superficie e vanno pesati a zero, o il bilineare sborda oltre la silhouette.
                if(ValidateHistoryTap(base + off[k], imageResultSize, isBackground, worldPos, aovNormal, ti, tn)) {
                    irrSum += w[k] * ti;
                    nSum   += w[k] * tn;
                    wSum   += w[k];
                }
            }
            if(wSum > 0.05) {
                histIrr = irrSum / wSum;
                histN = nSum / wSum;
                valid = true;
            }
        }
    }

    return valid;
}
