// --- Rumore di Perlin -------------------------------------------------------
// Perlin "improved" (2002): gradienti sugli spigoli della cella, fade quintico.
// Rispetto a RayTracingTheNextWeek non c'e' la tabella di permutazione (servirebbe
// un altro SSBO o un uniform array): i gradienti si ricavano da un hash intero
// della cella, che e' stateless e quindi piu' adatto a uno shader.
uint HashCell(ivec3 c) {
    uint h = uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(c.z) * 83492791u;
    h ^= h >> 13; h *= 1274126177u; h ^= h >> 16;
    return h;
}

// I 12 gradienti verso gli spigoli del cubo, senza array: e' la grad() di Perlin.
float GradDot(uint hash, vec3 d) {
    uint h = hash & 15u;
    float u = h < 8u ? d.x : d.y;
    float v = h < 4u ? d.y : (h == 12u || h == 14u ? d.x : d.z);
    return ((h & 1u) == 0u ? u : -u) + ((h & 2u) == 0u ? v : -v);
}

// Ritorna circa [-1, 1].
float PerlinNoise(vec3 p) {
    vec3 pf = floor(p);
    ivec3 i = ivec3(pf);
    vec3 f = p - pf;
    vec3 w = f * f * f * (f * (f * 6.0 - 15.0) + 10.0); // fade quintico: derivata seconda continua

    float n000 = GradDot(HashCell(i + ivec3(0,0,0)), f - vec3(0,0,0));
    float n100 = GradDot(HashCell(i + ivec3(1,0,0)), f - vec3(1,0,0));
    float n010 = GradDot(HashCell(i + ivec3(0,1,0)), f - vec3(0,1,0));
    float n110 = GradDot(HashCell(i + ivec3(1,1,0)), f - vec3(1,1,0));
    float n001 = GradDot(HashCell(i + ivec3(0,0,1)), f - vec3(0,0,1));
    float n101 = GradDot(HashCell(i + ivec3(1,0,1)), f - vec3(1,0,1));
    float n011 = GradDot(HashCell(i + ivec3(0,1,1)), f - vec3(0,1,1));
    float n111 = GradDot(HashCell(i + ivec3(1,1,1)), f - vec3(1,1,1));

    return mix(mix(mix(n000, n100, w.x), mix(n010, n110, w.x), w.y),
               mix(mix(n001, n101, w.x), mix(n011, n111, w.x), w.y), w.z);
}

// Somma di |perlin| a frequenze raddoppiate e ampiezze dimezzate: le creste
// prodotte dal valore assoluto sono quello che rende "sporco"/nuvoloso il risultato.
float Turbulence(vec3 p, int octaves) {
    float sum = 0.0;
    float weight = 1.0;
    for(int i = 0; i < octaves; i++) {
        sum += weight * abs(PerlinNoise(p));
        weight *= 0.5;
        p *= 2.0;
    }
    return sum;
}

// Scalare in [0, 1] che pilota il mix fra NoiseColorA e NoiseColorB.
float NoiseValue(Material material, vec3 position) {
    vec3 p = position * material.NoiseScale;
    int octaves = clamp(int(material.NoiseOctaves + 0.5), 1, 8);

    if(material.NoiseType > 2.5) {
        // Marmo: fase lineare su z che la turbolenza distorce -> venature.
        float phase = p.z + material.NoiseTurbulence * Turbulence(p, octaves);
        return 0.5 * (1.0 + sin(phase));
    }

    if(material.NoiseType > 1.5)
        return clamp(Turbulence(p, octaves), 0.0, 1.0);

    return 0.5 * (1.0 + PerlinNoise(p));
}

// Il colore albedo che moltiplica material.Color.
vec3 SampleAlbedo(Material material, vec2 uv, vec3 position, float lod) {
    vec3 albedo = vec3(1.0);

    // Scacchiera world-space: non dipende dalla parametrizzazione della superficie.
    // Indispensabile su una sfera enorme usata come terreno, dove le UV
    // equirettangolari degenerano in spicchi convergenti vicino ai poli.
    if(material.Checker > 1.5) {
        vec3 cell = floor(position * material.CheckerScale);
        float odd = mod(cell.x + cell.y + cell.z, 2.0);
        albedo = mix(vec3(0.05), vec3(0.95), odd);
    }
    // Scacchiera sulle UV: utile per validare la parametrizzazione senza asset.
    else if(material.Checker > 0.5) {
        vec2 cell = floor(uv * material.CheckerScale);
        float odd = mod(cell.x + cell.y, 2.0);
        albedo = mix(vec3(0.05), vec3(0.95), odd);
    }
    else if(material.AlbedoTexture >= 0.0) {
        albedo = textureLod(AlbedoTextures, vec3(uv, material.AlbedoTexture), lod).rgb;
    }

    // Il rumore si sovrappone all'albedo di base, qualunque esso sia: con Blend < 1
    // si sporca una texture o una scacchiera invece di sostituirla.
    if(material.NoiseType > 0.5) {
        float n = NoiseValue(material, position);
        vec3 noiseColor = mix(material.NoiseColorA.rgb, material.NoiseColorB.rgb, n);
        albedo = mix(albedo, noiseColor, clamp(material.NoiseBlend, 0.0, 1.0));
    }

    return albedo;
}
