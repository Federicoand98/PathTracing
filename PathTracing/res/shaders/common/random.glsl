uint rndSeed;

uint GetPCGHash(inout uint seed) {
    seed = seed * 747796405u + 2891336453u;
    uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    return (word >> 22u) ^ word;
}

float GetRandomFloat() {
    return float(GetPCGHash(rndSeed)) / 4294967296.0;
}

vec3 GetRandomVector(uint seed) {
    vec3 seedVec = vec3(
        float(seed & 0xFF) / 255.0,
        float((seed >> 8) & 0xFF) / 255.0,
        float((seed >> 16) & 0xFF) / 255.0
    );

    return vec3(
        fract(sin(dot(seedVec, vec3(12.9898, 78.233, 45.543))) * 43758.5453) * 2.0 - 1.0,
        fract(sin(dot(seedVec, vec3(39.3465, 93.535, 23.987))) * 43758.5453) * 2.0 - 1.0,
        fract(sin(dot(seedVec, vec3(67.123, 12.789, 98.456))) * 43758.5453) * 2.0 - 1.0
    );
}
