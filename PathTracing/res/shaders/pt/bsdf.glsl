// ---- BRDF microfacet GGX (Trowbridge-Reitz) + Fresnel RGB ------------------

// Fresnel di Schlick vettoriale: F0 e' un colore. Per un dielettrico e' grigio (~0.04),
// per un metallo e' l'albedo -> il riflesso e' tinto al centro e bianco ai bordi radenti.
vec3 FresnelSchlickRGB(vec3 F0, float cosTheta) {
    float m = clamp(1.0 - cosTheta, 0.0, 1.0);
    float m5 = (m * m) * (m * m) * m;
    return F0 + (1.0 - F0) * m5;
}

/*
*  n1: indice di rifrazione del materiale da cui proviene il raggio
*  n2: indice di rifrazione del materiale in cui entra il raggio
*  f0: valore di riflettanza del materiale ad angolo 0
*  f90: valore di riflettanza del materiale ad angolo 90
*/
float FresnelSchlick(float n1, float n2, vec3 normal, vec3 incident, float f0, float f90) {
        // Schlick aproximation
        float r0 = (n1-n2) / (n1+n2);
        r0 *= r0;
        float cosX = -dot(normal, incident);
        if (n1 > n2) {
            float n = n1/n2;
            float sinT2 = n*n*(1.0-cosX*cosX);
            // Total internal reflection
            if (sinT2 > 1.0)
                return f90;
            cosX = sqrt(1.0-sinT2);
        }
        float x = 1.0-cosX;
        float ret = r0+(1.0-r0)*x*x*x*x*x;

        // adjust reflect multiplier for object reflectivity
        return mix(f0, f90, ret);
}

// Base ortonormale branchless attorno a n (Duff et al. 2017): niente casi degeneri.
void BuildBasis(vec3 n, out vec3 t, out vec3 b) {
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float c = n.x * n.y * a;
    t = vec3(1.0 + s * n.x * n.x * a, s * c, -s * n.x);
    b = vec3(c, s + n.y * n.y * a, -n.y);
}

// Termine Lambda di Smith per GGX: entra nel masking-shadowing G.
float SmithLambda(float cosTheta, float alpha) {
    float c2 = cosTheta * cosTheta;
    float tan2 = (1.0 - c2) / max(c2, 1e-8);
    return 0.5 * (-1.0 + sqrt(1.0 + alpha * alpha * tan2));
}

// Sampling della distribuzione delle NORMALI VISIBILI (Heitz 2018): campiona solo micro-
// normali effettivamente viste da V, molta meno varianza del sampling della sola NDF.
// Spazio locale con la macro-normale = (0,0,1). alpha isotropo.
vec3 SampleGGXVNDF(vec3 Ve, float alpha, float u1, float u2) {
    vec3 Vh = normalize(vec3(alpha * Ve.x, alpha * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3 T2 = cross(Vh, T1);
    float r = sqrt(u1);
    float phi = 2.0 * PI * u2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(max(0.0, 1.0 - t1 * t1)) + s * t2;
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    return normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(1e-6, Nh.z)));
}

// Emisfero pesato sul coseno attorno a n: la pdf e' cos/PI, cosi' per un diffuso
// lambertiano il peso BRDF*cos/pdf si semplifica esattamente all'albedo.
vec3 CosineSampleHemisphere(vec3 n, float u1, float u2) {
    vec3 t, b;
    BuildBasis(n, t, b);
    float r = sqrt(u1);
    float phi = 2.0 * PI * u2;
    float z = sqrt(max(0.0, 1.0 - u1));
    return normalize(t * (r * cos(phi)) + b * (r * sin(phi)) + n * z);
}

// Deriva i parametri del BSDF dal materiale (nessun RNG). Estratto da BSDF cosi' che il
// sampling e la valutazione per una direzione arbitraria (NEE) usino la STESSA logica di
// selezione dei lobi: se divergessero, la MIS diventerebbe biased.
BsdfParams GetBsdfParams(HitInfo hit, Material material, float lod) {
    BsdfParams pr;
    pr.metalness = clamp(material.Metalness, 0.0, 1.0);
    vec3 albedo = SampleAlbedo(material, hit.UV, hit.HitPosition, lod);
    pr.baseColor = vec3(material.Color) * albedo;

    // F0: dielettrico grigio, metallo = albedo (vedi note storiche nel BSDF).
    float dielF0 = min(material.SpecularProbability, 0.08);
    vec3 tintHue = pr.baseColor / max(Luminance(pr.baseColor), 1e-4);
    vec3 dielSpec = dielF0 * mix(vec3(1.0), tintHue, clamp(material.SpecularTint, 0.0, 1.0));
    pr.F0 = mix(dielSpec, pr.baseColor, pr.metalness);

    float roughness = clamp(material.Roughness, 0.0, 1.0);
    pr.alpha = max(roughness * roughness, 1e-3);

    pr.pRefr = material.RefractionProbability * (1.0 - pr.metalness);
    float specImp = Luminance(pr.F0);
    float diffImp = Luminance(pr.baseColor) * (1.0 - pr.metalness);
    float budget = 1.0 - pr.pRefr;
    pr.pSpec = (diffImp <= 0.0) ? budget
             : budget * clamp(specImp / max(specImp + diffImp, 1e-4), 0.05, 0.95);
    pr.pDiff = budget - pr.pSpec;
    return pr;
}

// pdf (in solid angle) che il sampling del BSDF userebbe per la direzione L. E' la pdf
// COMBINATA dei due lobi non-delta pesati per la loro prob. di selezione: il sampler poteva
// generare L via lo speculare O il diffuso, e la MIS ha bisogno della densita' totale.
float CombinedPdf(BsdfParams pr, vec3 N, vec3 V, vec3 L) {
    float NoL = dot(N, L);
    float NoV = dot(N, V);
    if(NoL <= 0.0 || NoV <= 0.0)
        return 0.0;

    // lobo speculare: pdf VNDF(L) = D(H) * G1(V) / (4 * NoV)
    vec3 H = normalize(V + L);
    float NoH = max(dot(N, H), 0.0);
    float a2 = pr.alpha * pr.alpha;
    float denom = NoH * NoH * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * denom * denom, 1e-12);
    float G1 = 1.0 / (1.0 + SmithLambda(NoV, pr.alpha));
    float pdfSpec = D * G1 / max(4.0 * NoV, 1e-8);

    float pdfDiff = NoL / PI; // cosine-weighted

    return pr.pDiff * pdfDiff + pr.pSpec * pdfSpec;
}

// Valuta il BSDF (parte non-delta: diffuso + speculare GGX) verso una direzione L arbitraria,
// restituendo f (valore BRDF, NON moltiplicato per il coseno) e la pdf combinata. Usata dalla
// NEE per il termine di light sampling.
void EvalBsdf(BsdfParams pr, vec3 N, vec3 V, vec3 L, out vec3 f, out float pdf) {
    f = vec3(0.0);
    pdf = 0.0;
    float NoL = dot(N, L);
    float NoV = dot(N, V);
    if(NoL <= 0.0 || NoV <= 0.0)
        return;

    vec3 fDiff = pr.baseColor * (1.0 - pr.metalness) / PI;

    vec3 H = normalize(V + L);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);
    float a2 = pr.alpha * pr.alpha;
    float denom = NoH * NoH * (a2 - 1.0) + 1.0;
    float D = a2 / max(PI * denom * denom, 1e-12);
    float Lv = SmithLambda(NoV, pr.alpha);
    float Ll = SmithLambda(NoL, pr.alpha);
    float G2 = 1.0 / (1.0 + Lv + Ll);
    vec3 F = FresnelSchlickRGB(pr.F0, VoH);
    vec3 fSpec = D * G2 * F / max(4.0 * NoV * NoL, 1e-8);

    f = fDiff + fSpec;
    pdf = CombinedPdf(pr, N, V, L);
}

// Ritorna il peso del campione (throughput multiplier) e imposta ray.Origin/Direction.
// outPdf = pdf (solid angle) della direzione scelta, per la MIS sul prossimo hit emissivo.
// outDelta = true solo per il lobo di rifrazione (distribuzione delta): la NEE lo salta e la
// sua emissione vale peso 1. Lo speculare GGX, anche a roughness bassa, resta non-delta: la
// MIS lo gestisce senza soglie (pdf enorme -> peso ~1 in automatico).
vec3 BSDF(inout Ray ray, HitInfo hit, Material material, BsdfParams pr, out float outPdf, out bool outDelta) {
    outPdf = 0.0;
    outDelta = false;

    vec3 N = hit.Normal;         // orientata verso il viewer (vedi HandleHit / flip Inside)
    vec3 V = -ray.Direction;     // dal punto verso la camera
    vec3 hitPos = ray.Origin + ray.Direction * hit.HitDistance;

    float roll = GetRandomFloat();

    // ---- lobo di rifrazione (vetro): delta, nessuna NEE, emissione a valle con peso 1 ----
    if(pr.pRefr > 0.0 && roll < pr.pRefr) {
        outDelta = true;
        float n1 = hit.Inside ? material.RefractionRatio : 1.0;
        float n2 = hit.Inside ? 1.0 : material.RefractionRatio;
        float F = FresnelSchlick(n1, n2, N, ray.Direction, 0.0, 1.0);
        vec3 dir = refract(ray.Direction, N, n1 / n2);

        // Interfaccia dielettrica: una frazione F dell'energia si riflette. refract() vale 0 in
        // riflessione interna totale: va gestita qui, normalize(0) produrrebbe un NaN che
        // cancella l'intero campione e scurisce il vetro.
        float side = -1.0;
        if(dot(dir, dir) < 1e-8 || GetRandomFloat() < F) {
            dir = reflect(ray.Direction, N);
            side = 1.0;
        }

        ray.Origin = hitPos + hit.GeoNormal * 0.001 * side;
        ray.Direction = normalize(mix(dir, normalize(side * N + GetRandomVector(rndSeed)),
                                      material.RefractionRoughness * material.RefractionRoughness));
        return vec3(1.0 / pr.pRefr); // la tinta arriva dal Beer-Lambert nel segmento interno
    }

    ray.Origin = hitPos + hit.GeoNormal * 0.001;

    if(roll < pr.pRefr + pr.pSpec) {
        // ---- speculare: microfacet GGX con VNDF ----
        vec3 T, B;
        BuildBasis(N, T, B);
        vec3 Vl = vec3(dot(V, T), dot(V, B), dot(V, N)); // V in spazio locale (Vl.z = NoV)
        vec3 Hl = SampleGGXVNDF(Vl, pr.alpha, GetRandomFloat(), GetRandomFloat());
        vec3 Ll = reflect(-Vl, Hl); // direzione di luce riflessa attorno alla micro-normale

        if(Ll.z <= 0.0)             // sotto l'orizzonte: campione non valido
            return vec3(0.0);

        float NoV = max(Vl.z, 1e-4);
        float NoL = max(Ll.z, 1e-4);
        float VoH = max(dot(Vl, Hl), 0.0);

        // Peso VNDF: BRDF*cos/pdf = F * G2/G1. Con Smith height-correlated
        // G2/G1 = (1+Lv) / (1+Lv+Ll). La NDF si cancella nel rapporto.
        vec3 F = FresnelSchlickRGB(pr.F0, VoH);
        float Lv = SmithLambda(NoV, pr.alpha);
        float Ll_ = SmithLambda(NoL, pr.alpha);
        float gRatio = (1.0 + Lv) / (1.0 + Lv + Ll_);

        ray.Direction = normalize(T * Ll.x + B * Ll.y + N * Ll.z);
        outPdf = CombinedPdf(pr, N, V, ray.Direction);
        return F * gRatio / pr.pSpec;
    }
    else {
        // ---- diffuso lambertiano, cosine-weighted ----
        ray.Direction = CosineSampleHemisphere(N, GetRandomFloat(), GetRandomFloat());
        outPdf = CombinedPdf(pr, N, V, ray.Direction);
        return pr.baseColor * (1.0 - pr.metalness) / pr.pDiff;
    }
}
