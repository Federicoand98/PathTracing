// Contratto del display: imgOutput contiene sempre BEAUTY (colore finale lineare). Non e' piu'
// l'accumulatore: e' derivato ogni frame rimodulando l'irradianza accumulata. Vedi ADR 0002.
layout(rgba32f, binding = 0) uniform restrict writeonly image2D imgOutput;

// AOV (feature buffer) del primo hit, del FRAME CORRENTE (non piu' accumulati nel tempo:
// la riproiezione ha bisogno delle feature di questo frame per validare la storia, e
// albedo/normale/depth sono deterministici, non hanno varianza da mediare via).
// Normale e depth stanno insieme: il limite e' 8 image unit e la storia ne raddoppia il conto.
layout(rgba32f, binding = 1) uniform restrict image2D imgAlbedo;      // rgb = base color
layout(rgba32f, binding = 2) uniform restrict image2D imgNormalDepth; // rgb = normale world, a = depth

// L'ACCUMULATORE vero: integra irradianza (luce senza dettaglio di superficie), non beauty.
// Riproiettare il beauty porterebbe dietro anche l'albedo del pixel vecchio -> ghosting di
// texture. Nell'alpha il sample count N, che e' PER-PIXEL perche' la disocclusione azzera la
// storia solo in alcuni pixel. Vedi ADR 0002.
layout(rgba32f, binding = 3) uniform restrict writeonly image2D imgIrradiance;  // rgb = irradianza, a = N

// La STORIA: gli stessi due buffer del frame precedente, in sola lettura. Ping-pong lato C++.
layout(rgba32f, binding = 4) uniform restrict readonly image2D imgNormalDepthPrev;
layout(rgba32f, binding = 5) uniform restrict readonly image2D imgIrradiancePrev;

uniform samplerCube SamplerEnvironment;

// Un array invece di N sampler2D: l'indice della texture dipende dal materiale colpito,
// che varia da raggio a raggio. Indicizzare un array di sampler con un indice non
// "dynamically uniform" e' undefined behavior; il layer di un sampler2DArray no.
uniform sampler2DArray AlbedoTextures;

uniform int rendererFrame;
uniform int samplesPerPixel;
uniform int rayDepth;
uniform vec3 cameraPosition;
uniform mat4 inverseProjection;
uniform mat4 inverseView;
uniform vec3 BackgroundColor;
uniform bool EnvironmentMapping;
uniform int bvhDebug;         // 0 = off, 1 = heatmap del costo di traversata BVH
uniform float bvhHeatScale;   // n. di nodi visitati che mappa al colore "caldo" (rosso)
uniform int aovView;          // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth (vista di debug)
uniform ivec2 pickPixel;      // pixel da campionare per il picking, (-1,-1) = nessuno
uniform float fireflyClamp;   // luminanza massima per campione, <= 0 = off
uniform float apertureRadius; // raggio della lente (thin-lens). <= 0 = pinhole, niente DOF
uniform float focusDistance;  // distanza del piano di fuoco lungo il raggio

// Riproiezione temporale (ADR 0002). La camera del frame PRECEDENTE: serve sia a ritrovare il
// pixel nella storia (prevViewProjection) sia a validarla (prevCameraPosition: il depth in
// storia e' misurato da li', non dalla camera di adesso).
uniform mat4 prevViewProjection;
uniform vec3 prevCameraPosition;
uniform int aovNeedsUpdate; // 1 = camera cambiata/reset: gli AOV vanno ritracciati
uniform int hasHistory;    // 0 = niente storia valida (primo frame, reset scena, resize)
uniform int cameraMoved;   // 1 = la storia e' riproiettata -> va cappata
uniform int historyCap;    // lunghezza massima della storia quando ci si muove
uniform int numLights;     // emettitori per la NEE; 0 = NEE disattivata
uniform int primBvhRoot;   // radice del BVH di sfere/quad in BVH4Nodes; < 0 = assente
uniform int tlasRoot;      // radice del TLAS in BVH4Nodes; < 0 = assente

layout(std430, binding = 0) restrict readonly buffer SpheresBuffer {
    Sphere Spheres[];
};

layout(std430, binding = 1) restrict readonly buffer MaterialsBuffer {
    Material Materials[];
};

layout(std430, binding = 2) restrict readonly buffer QuadsBuffer {
    Quad Quads[];
};

layout(std430, binding = 3) restrict readonly buffer TrianglePositionsBuffer {
    TrianglePosition TriPositions[];
};

layout(std430, binding = 4) restrict readonly buffer MeshBuffer {
    MeshInfo Meshes[];
};

layout(std430, binding = 5) restrict readonly buffer CubeBuffer {
    CubeInfo Cubes[];
};

layout(std430, binding = 6) restrict readonly buffer BVHBuffer {
    BVH4Node BVH4Nodes[];
};

layout(std430, binding = 7) restrict readonly buffer TriIndexBuffer {
    int TriIndex[];
};

layout(std430, binding = 8) restrict readonly buffer TriangleNormalsBuffer {
    TriangleNormal TriNormals[];
};

// Risultato del picking: scritto da una sola invocazione (quella su pickPixel),
// quindi non serve nessuna atomica.
layout(std430, binding = 9) restrict writeonly buffer PickBuffer {
    int pickType;   // 0 sfera, 1 quad, 2 mesh, -1 niente
    int pickIndex;
    float pickDistance; // t del raggio primario al punto colpito, < 0 = niente
};

layout(std430, binding = 10) restrict readonly buffer TriangleUVsBuffer {
    TriangleUV TriUVs[];
};

// Emettitori per la NEE (light sampling + MIS). Vuoto (numLights == 0) su tutte le scene
// senza geometria emissiva -> la NEE si disattiva del tutto: nessun costo, render identico.
layout(std430, binding = 11) restrict readonly buffer LightsBuffer {
    Light Lights[];
};

#if USE_PRIM_BVH || USE_TLAS
// Foglie dei BVH di primo livello. Primitive analitiche: (indice << 1) | tipo (0 sfera, 1 quad).
// TLAS: indice della mesh.
layout(std430, binding = 12) restrict readonly buffer PrimIndexBuffer {
    int PrimIndex[];
};
#endif
