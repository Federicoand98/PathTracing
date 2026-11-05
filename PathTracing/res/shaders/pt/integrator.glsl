// Punto uniforme nel disco unitario (mappatura concentrica polare, niente rejection).
vec2 RandomInUnitDisk() {
    float r = sqrt(GetRandomFloat());
    float theta = 2.0 * PI * GetRandomFloat();
    return r * vec2(cos(theta), sin(theta));
}

// Livello di mip dopo il primo rimbalzo. Il dettaglio fine di una texture vista per riflesso
// indiretto si media comunque via: un mip ridotto da' lo stesso valore atteso con molti meno
// cache miss, perche' i raggi secondari sono incoerenti e saltano per tutta la texture.
const float SECONDARY_TEXTURE_LOD = 2.0;

vec3 PerPixel(vec2 ndc) {
    Ray ray = CalculateRayFromCamera(inverseProjection, inverseView, cameraPosition, ndc);

    // Depth of field, modello thin-lens. Il raggio pinhole parte dal centro camera; con
    // apertura > 0 lo faccio partire da un punto sul disco della lente e lo ripunto verso
    // il punto di fuoco (a focusDistance lungo il raggio pinhole). Ogni campione pesca un
    // punto diverso della lente: mediandoli nasce lo sfocato. Va SOLO qui, nel raggio di
    // render: il picking e il BVH debug restano pinhole (altrimenti il pick sarebbe impreciso).
    if(apertureRadius > 0.0) {
        vec3 focusPoint = ray.Origin + ray.Direction * focusDistance;

        // assi camera in world space: colonne dell'inversa della view
        vec3 right = normalize(vec3(inverseView[0]));
        vec3 up    = normalize(vec3(inverseView[1]));

        vec2 lens = apertureRadius * RandomInUnitDisk();
        vec3 origin = ray.Origin + right * lens.x + up * lens.y;

        ray = Ray(origin, normalize(focusPoint - origin), 0);
    }

    Material material;
    vec3 light = vec3(0.0);          // radianza accumulata lungo il cammino
    vec3 throughput = vec3(1.0);     // prodotto dei pesi BSDF (e RR) fin qui

    // Stato per la MIS sull'emissione: quando un raggio BSDF cade su un emettitore, il suo
    // contributo va pesato contro la pdf che il light sampling avrebbe avuto DAL VERTICE
    // PRECEDENTE. Il raggio camera si comporta come una sorgente delta (peso 1).
    float prevPdfBsdf = 0.0;
    bool  prevDelta = true;
    vec3  prevPos = ray.Origin;

    for(int i = 0; i < rayDepth; i++) {
        HitInfo hit = TraceRay(ray, i == 0 ? 0.0 : SECONDARY_TEXTURE_LOD);

        if(hit.HitDistance < 0.0) {
            // L'ambiente non e' nel set NEE (sfondo uniforme -> campionarlo non aiuta): peso 1.
            // EnvironmentMapping: la cubemap, altrimenti il colore piatto di BackgroundColor.
            vec3 envRadiance = EnvironmentMapping
                ? textureLod(SamplerEnvironment, ray.Direction, 0.0).rgb
                : BackgroundColor;
            light += envRadiance * throughput;

            // Il Sole visto da un raggio BSDF (ADR 0006): stessa MIS degli altri Emitter. Dopo un
            // lobo delta, o con la NEE spenta, la NEE non poteva trovarlo: peso 1.
            if(sunRadiance.w > 0.0 && dot(ray.Direction, sunDirection.xyz) >= sunDirection.w) {
                float w = 1.0;
                if(!prevDelta && numLights > 0) {
                    float pdfL = (1.0 / float(numLights)) / sunRadiance.w;
                    w = prevPdfBsdf / (prevPdfBsdf + pdfL);
                }
                light += sunRadiance.rgb * throughput * w;
            }
            break;
        }

        // Materiale gia' risolto in HandleHit (per-triangolo sulle mesh). Il clamp protegge
        // da indici fuori range, es. un MTL malformato in un OBJ multi-materiale.
        material = Materials[clamp(hit.MaterialIndex, 0, numMaterials - 1)];

        if(hit.Inside) {
            hit.Normal *= -1.0;
            hit.GeoNormal *= -1.0;
            // Beer-Lambert: la trasmittanza sul segmento interno e' RefractionColor^distanza,
            // cioe' RefractionColor e' il colore trasmesso a distanza unitaria (rosso -> vetro
            // rosso; bianco -> trasparente). L'assorbimento e' -log(colore): usare RefractionColor
            // direttamente come coefficiente invertirebbe la tinta (rosso -> ciano).
            vec3 absorption = -log(clamp(vec3(material.RefractionColor), vec3(1e-3), vec3(1.0)));
            throughput *= exp(-absorption * hit.HitDistance);
        }

        vec3 N = hit.Normal;
        vec3 V = -ray.Direction;

        // (1) EMISSIONE della superficie, pesata in MIS. Senza NEE (numLights==0 o vertice
        //     dopo un lobo delta) il peso e' 1 e il comportamento coincide con quello storico.
        if(material.EmissiveStrenght > 0.0) {
            vec3 Le = vec3(material.EmissiveColor) * material.EmissiveStrenght;
            float w = 1.0;
            if(!prevDelta && numLights > 0) {
                float pdfL = LightPdfToward(prevPos, hit);
                float sum = prevPdfBsdf + pdfL;
                w = (sum > 0.0) ? prevPdfBsdf / sum : 1.0;
            }
            light += Le * throughput * w;
        }

        // Un solo calcolo per vertice, condiviso da NEE e sampling: l'albedo puo' costare una
        // texture fetch piu' fino a 8 ottave di rumore.
        BsdfParams pr = GetBsdfParams(hit, material, i == 0 ? 0.0 : SECONDARY_TEXTURE_LOD);

        // (2) NEE: campiona una luce e aggiungi il contributo diretto (solo superfici esterne;
        //     dentro un mezzo la rifrazione all'uscita renderebbe il sample errato).
        if(numLights > 0 && !hit.Inside) {
            vec3 wi; float dist; vec3 Lrad; float pdfLight; bool deltaLight;
            if(SampleLight(hit.HitPosition, wi, dist, Lrad, pdfLight, deltaLight) && pdfLight > 1e-8) {
                // Col normal map la luce deve stare sopra anche la superficie vera, non solo
                // sopra la normale di shading: altrimenti illuminerebbe da sotto (ADR 0007).
                if(dot(N, wi) > 0.0 && (material.NormalTexture < 0.0 || dot(hit.GeoNormal, wi) > 0.0)) {
                    vec3 f; float pdfBsdfDir;
                    EvalBsdf(pr, N, V, wi, f, pdfBsdfDir);
                    if(any(greaterThan(f, vec3(0.0)))) {
                        vec3 sorigin = hit.HitPosition + hit.GeoNormal * 0.001;
                        if(!Occluded(sorigin, wi, dist - 0.002)) {
                            // una luce delta la trova solo la NEE: niente da bilanciare
                            float w = deltaLight ? 1.0 : pdfLight / (pdfLight + pdfBsdfDir);
                            light += throughput * f * dot(N, wi) * Lrad / pdfLight * w;
                        }
                    }
                }
            }
        }

        // (3) Campiona il BSDF -> prossima direzione. Il peso e' gia' BRDF*cos/pdf.
        float pdfBsdf; bool delta;
        vec3 bsdfWeight = BSDF(ray, hit, material, pr, pdfBsdf, delta);

        throughput *= bsdfWeight;

        // Una direzione sopra la normale di shading ma sotto la superficie vera attraverserebbe
        // la geometria: il cammino si chiude (ADR 0007). Senza normal map le due normali
        // coincidono e il caso non esiste.
        if(!delta && material.NormalTexture >= 0.0 && dot(ray.Direction, hit.GeoNormal) <= 0.0)
            break;
        prevPdfBsdf = pdfBsdf;
        prevDelta = delta;
        prevPos = hit.HitPosition;

        // Russian Roulette: i cammini a basso throughput vengono terminati e i sopravvissuti
        // reintegrano l'energia persa. p e' una PROBABILITA': va limitata a 1, altrimenti con
        // throughput > 1 (es. il peso F/pSpec ai bordi radenti) il cammino sopravvive sempre
        // ma viene comunque diviso per p, perdendo energia.
        {
            float p = min(max(throughput.x, max(throughput.y, throughput.z)), 1.0);
            if(p <= 0.0 || GetRandomFloat() > p)
                break;
            throughput *= 1.0 / p;
        }
    }

    return light;
}
