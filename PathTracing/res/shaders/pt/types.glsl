struct Ray {
    vec3 Origin;
    vec3 Direction;
    float t;
};

struct Sphere {
    vec4 Position;
    float Mat;
};

struct Quad {
	vec4 PositionLLC;
	vec4 U;
	vec4 V;
	float MaterialIndex;
	float Width;
	float Height;
};

struct MeshInfo {
    mat4 Transform;
    mat4 InvTransform;
    vec4 BoundsMin;   // AABB in world space
    vec4 BoundsMax;
    vec4 LocalMin;
    vec4 LocalMax;
    float FirstTriangle;
    float NumTriangles;
    float MaterialIndex;
    float NumOpaque;  // i primi NumOpaque triangoli sono la geometria opaca del BLAS, gli altri la mascherata
};

// Posizioni e normali sono in due buffer separati: le posizioni sono anche l'ingresso dei BLAS,
// le normali servono una sola volta sull'hit finale.
struct TrianglePosition {
    vec4 A;
    vec4 B;
    vec4 C;
};

struct TriangleNormal {
    vec4 NormalA;
    vec4 NormalB;
    vec4 NormalC;
};

// uv nel .xy; anche questo buffer e' freddo, letto solo sull'hit finale
struct TriangleUV {
    vec4 A;
    vec4 B;
    vec4 C;
};

struct Material {
    vec4 Color;
    float Roughness;
    float SpecularProbability;
    float RefractionRatio;
    float EmissiveStrenght;
    vec4 EmissiveColor;
	vec4 SpecularColor;
	float RefractionProbability;
	float RefractionRoughness;
	vec4 RefractionColor;
    float AlbedoTexture; // layer nel sampler2DArray, < 0 = nessuna texture
    float Checker;       // 0 = off, 1 = scacchiera UV, 2 = scacchiera world-space
    float CheckerScale;  // celle per unita' di UV (o di spazio, se world-space)
    float NoiseType;       // 0 = off, 1 = perlin, 2 = turbolenza, 3 = marmo
    float NoiseScale;      // frequenza in celle per unita' di mondo
    float NoiseBlend;      // 0 = albedo di base, 1 = solo rumore
    float NoiseOctaves;    // ottave della turbolenza
    float NoiseTurbulence; // peso della turbolenza nella fase del marmo
    vec4 NoiseColorA;
    vec4 NoiseColorB;
    float Metalness;     // 0 = dielettrico, 1 = metallo
    float SpecularTint;  // 0 = riflesso bianco, 1 = tinto verso base color (solo dielettrici)
    float NormalTexture;  // layer nel sampler2DArray DataTextures, < 0 = nessun normal map
    float NormalStrength; // scala la pendenza del normal map: 0 = piatto, 1 = come authored
    float NormalFlipY;    // 1 = canale verde invertito (convenzione DirectX)
    float AlphaTexture;   // layer in DataTextures (canale rosso), < 0 = alpha dell'albedo
    float AlphaCutoff;    // > 0 = mascherato: il raggio passa dove l'alpha e' sotto la soglia
};

// Emettitore per la Next Event Estimation. Deve combaciare con GPULight (Primitives.h).
// Per un quad U/V sono gli spigoli FISICI e area = |U x V| (precalcolati lato CPU), cosi'
// il campionamento e' q = P.xyz + s*U + t*V con s,t in [0,1].
struct Light {
    vec4 Emission;  // rgb = radianza, a = area
    vec4 P;         // sfera: centro.xyz + raggio(w) ; quad: LLC.xyz
    vec4 U;         // quad: spigolo U ; sfera: inutilizzato
    vec4 V;         // quad: spigolo V ; sfera: inutilizzato
    vec4 Normal;    // quad: normale ; sfera: inutilizzato
    ivec4 Meta;     // x = tipo (0 sfera, 1 quad, 2 Sole, 3 punto, 4 spot), y = objType, z = objIndex
};

// Parametri del BSDF derivati dal materiale (nessun RNG): condivisi tra il sampling (BSDF)
// e la valutazione per una direzione arbitraria (EvalBsdf/CombinedPdf), cosi' la logica di
// selezione dei lobi non puo' divergere tra i due usi.
struct BsdfParams {
    vec3 baseColor;
    vec3 F0;
    float alpha;
    float metalness;
    float pSpec;   // prob. di scegliere il lobo speculare
    float pDiff;   // prob. di scegliere il lobo diffuso
    float pRefr;   // prob. di scegliere il lobo di rifrazione (delta)
};

struct HitInfo {
    float HitDistance;
    vec3 HitPosition;
    vec3 Normal;
    int ObjectIndex;
    int ObjectType;
    bool Inside;
    vec2 UV;
    int MaterialIndex; // materiale gia' risolto: per-triangolo sulle mesh, altrimenti dell'oggetto
    // Normal e' la normale di SHADING (col normal map, se c'e'); GeoNormal quella senza normal
    // map, orientata allo stesso modo. Gli offset dei raggi e il test d'orizzonte usano GeoNormal:
    // la normale perturbata puo' puntare sotto la superficie vera (ADR 0007).
    vec3 GeoNormal;
};
