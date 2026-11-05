// ---- NEE: sampling delle luci e pdf per la MIS --------------------------------

// Distanza a cui si mette il Sole per il raggio d'ombra: e' all'infinito, ma basta oltre
// qualunque geometria. Combacia con il tMax di TraceRay.
const float SUN_DISTANCE = 100000.0;

// Campiona un emettitore (scelto uniformemente) e restituisce direzione, distanza, radianza
// e pdf in solid angle (gia' comprensiva di 1/numLights). false = campione non valido.
// delta = luce punto o spot: nessun raggio BSDF la puo' colpire, quindi peso MIS 1. Per loro
// Lrad e' gia' l'irradianza sul punto (I / d^2) e pdf la sola probabilita' di sceglierla.
bool SampleLight(vec3 p, out vec3 wi, out float dist, out vec3 Lrad, out float pdf, out bool delta) {
    wi = vec3(0.0, 1.0, 0.0); dist = 0.0; Lrad = vec3(0.0); pdf = 0.0; delta = false;
    if(numLights == 0)
        return false;

    int li = min(int(GetRandomFloat() * float(numLights)), numLights - 1);
    Light L = Lights[li];
    Lrad = L.Emission.rgb;
    float pickPdf = 1.0 / float(numLights);

    if(L.Meta.x >= 3) {
        // ---- luce punto o spot (ADR 0009): una sola direzione possibile ----
        vec3 w = L.P.xyz - p;
        float d2 = dot(w, w);
        if(d2 < 1e-8)
            return false;
        dist = sqrt(d2);
        wi = w / dist;
        float falloff = 1.0;
        if(L.Meta.x == 4) {
            // sfumatura del cono come nella specifica glTF: lineare in cos, poi al quadrato
            float cd = dot(L.U.xyz, -wi);
            float t = clamp((cd - L.U.w) / max(L.V.x - L.U.w, 1e-3), 0.0, 1.0);
            falloff = t * t;
            if(falloff <= 0.0)
                return false;
        }
        Lrad *= falloff / d2;
        pdf = pickPdf;
        delta = true;
        return true;
    }

    if(L.Meta.x == 2) {
        // ---- Sole: cono uniforme attorno alla sua direzione, uguale da ogni punto ----
        float cosMax = L.P.w;
        float cosT = 1.0 - GetRandomFloat() * (1.0 - cosMax);
        float sinT = sqrt(max(0.0, 1.0 - cosT * cosT));
        float phi = 2.0 * PI * GetRandomFloat();

        vec3 tx, ty; BuildBasis(L.P.xyz, tx, ty);
        wi = normalize(tx * (sinT * cos(phi)) + ty * (sinT * sin(phi)) + L.P.xyz * cosT);
        dist = SUN_DISTANCE;
        pdf = pickPdf / max(L.Emission.a, 1e-12);
        return true;
    }

    if(L.Meta.x == 0) {
        // ---- sfera: cone sampling (Shirley) ----
        vec3 c = L.P.xyz; float r = L.P.w;
        vec3 w = c - p;
        float d2 = dot(w, w);
        float d = sqrt(d2);
        if(d <= r)               // punto dentro la sfera-luce: caso degenere, salta
            return false;

        float cosMax = sqrt(max(0.0, 1.0 - (r * r) / d2));
        float u1 = GetRandomFloat(), u2 = GetRandomFloat();
        float cosT = 1.0 - u1 * (1.0 - cosMax);
        float sinT = sqrt(max(0.0, 1.0 - cosT * cosT));
        float phi = 2.0 * PI * u2;

        vec3 wz = w / d;
        vec3 tx, ty; BuildBasis(wz, tx, ty);
        wi = normalize(tx * (sinT * cos(phi)) + ty * (sinT * sin(phi)) + wz * cosT);

        // distanza all'intersezione con la sfera lungo wi (radice vicina)
        float b = dot(wi, p - c);
        float disc = b * b - (d2 - r * r);
        dist = (disc >= 0.0) ? (-b - sqrt(disc)) : (d - r);
        if(dist <= 0.0)
            dist = d - r;

        float solid = 2.0 * PI * (1.0 - cosMax);
        pdf = pickPdf / max(solid, 1e-8);
        return true;
    } else {
        // ---- quad: area sampling ----
        float u1 = GetRandomFloat(), u2 = GetRandomFloat();
        vec3 q = L.P.xyz + u1 * L.U.xyz + u2 * L.V.xyz;
        vec3 dvec = q - p;
        float dist2 = dot(dvec, dvec);
        dist = sqrt(dist2);
        wi = dvec / dist;
        float cosLgt = abs(dot(L.Normal.xyz, wi)); // emettitore two-sided (come l'emissione sull'hit)
        if(cosLgt < 1e-6)
            return false;
        pdf = pickPdf * dist2 / (cosLgt * L.Emission.a);
        return true;
    }
}

// pdf (solid angle, incl. 1/numLights) con cui il light sampling avrebbe generato la direzione
// da fromPos verso l'emettitore effettivamente colpito da un raggio BSDF. Serve al termine MIS
// sull'emissione. 0 se l'oggetto colpito non e' un emettitore campionabile.
float LightPdfToward(vec3 fromPos, HitInfo hit) {
    if(numLights == 0)
        return 0.0;
    for(int i = 0; i < numLights; i++) {
        if(Lights[i].Meta.y != hit.ObjectType || Lights[i].Meta.z != hit.ObjectIndex)
            continue;
        float pickPdf = 1.0 / float(numLights);
        if(Lights[i].Meta.x == 0) {
            vec3 c = Lights[i].P.xyz; float r = Lights[i].P.w;
            float d2 = dot(c - fromPos, c - fromPos);
            if(d2 <= r * r)
                return 0.0;
            float cosMax = sqrt(max(0.0, 1.0 - (r * r) / d2));
            float solid = 2.0 * PI * (1.0 - cosMax);
            return pickPdf / max(solid, 1e-8);
        } else {
            vec3 dvec = hit.HitPosition - fromPos;
            float dist2 = dot(dvec, dvec);
            float cosLgt = abs(dot(Lights[i].Normal.xyz, normalize(dvec)));
            if(cosLgt < 1e-6)
                return 0.0;
            return pickPdf * dist2 / (cosLgt * Lights[i].Emission.a);
        }
    }
    return 0.0;
}
