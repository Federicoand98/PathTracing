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

layout(binding = 6) uniform samplerCube SamplerEnvironment;

// Un array invece di N sampler2D: l'indice della texture dipende dal materiale colpito,
// che varia da raggio a raggio. Indicizzare un array di sampler con un indice non
// "dynamically uniform" e' undefined behavior; il layer di un sampler2DArray no.
layout(binding = 7) uniform sampler2DArray AlbedoTextures;

// Parametri del frame. Deve combaciare con FrameParams in Renderer/PathTracer.h. Il blocco e'
// anonimo: i campi si usano per nome diretto. Layout std140 coi vec3 chiusi da uno scalare.
layout(std140, binding = 8) uniform FrameParams {
    mat4 inverseProjection;
    mat4 inverseView;
    // Riproiezione temporale (ADR 0002). La camera del frame PRECEDENTE: serve sia a ritrovare il
    // pixel nella storia (prevViewProjection) sia a validarla (prevCameraPosition: il depth in
    // storia e' misurato da li', non dalla camera di adesso).
    mat4 prevViewProjection;
    vec3 cameraPosition;
    int rendererFrame;
    vec3 prevCameraPosition;
    int samplesPerPixel;
    vec3 BackgroundColor;
    int rayDepth;
    // Il Sole (ADR 0006), anche fuori dalla light list: un raggio BSDF che lo colpisce deve
    // vederlo pure con la NEE spenta, come vede le superfici emissive.
    vec4 sunDirection;    // xyz = verso il Sole, w = cos del raggio angolare
    vec4 sunRadiance;     // rgb = radianza del disco, w = angolo solido (0 = nessun Sole)
    ivec2 pickPixel;      // pixel da campionare per il picking, (-1,-1) = nessuno
    int aovView;          // 0 = beauty, 1 = albedo, 2 = normal, 3 = depth (vista di debug)
    int numLights;        // emettitori per la NEE; 0 = NEE disattivata
    float fireflyClamp;   // luminanza massima per campione, <= 0 = off
    float apertureRadius; // raggio della lente (thin-lens). <= 0 = pinhole, niente DOF
    float focusDistance;  // distanza del piano di fuoco lungo il raggio
    int aovNeedsUpdate;   // 1 = camera cambiata/reset: gli AOV vanno ritracciati
    int hasHistory;       // 0 = niente storia valida (primo frame, reset scena, resize)
    int cameraMoved;      // 1 = la storia e' riproiettata -> va cappata
    int historyCap;       // lunghezza massima della storia quando ci si muove
    bool EnvironmentMapping;
    // Vulkan non ammette buffer vuoti: una collezione vuota ha comunque un buffer, quindi la
    // dimensione non si puo' ricavare da .length().
    int numSpheres;       // quanti primitivi del BLAS procedurale sono sfere: i seguenti sono quad
    int numMaterials;
};

layout(std430, binding = 9) restrict readonly buffer SpheresBuffer {
    Sphere Spheres[];
};

layout(std430, binding = 10) restrict readonly buffer MaterialsBuffer {
    Material Materials[];
};

layout(std430, binding = 11) restrict readonly buffer QuadsBuffer {
    Quad Quads[];
};

layout(std430, binding = 12) restrict readonly buffer TrianglePositionsBuffer {
    TrianglePosition TriPositions[];
};

layout(std430, binding = 13) restrict readonly buffer MeshBuffer {
    MeshInfo Meshes[];
};

layout(std430, binding = 14) restrict readonly buffer TriangleNormalsBuffer {
    TriangleNormal TriNormals[];
};

// Risultato del picking: scritto da una sola invocazione (quella su pickPixel),
// quindi non serve nessuna atomica.
layout(std430, binding = 15) restrict writeonly buffer PickBuffer {
    int pickType;   // 0 sfera, 1 quad, 2 mesh, -1 niente
    int pickIndex;
    float pickDistance; // t del raggio primario al punto colpito, < 0 = niente
};

layout(std430, binding = 16) restrict readonly buffer TriangleUVsBuffer {
    TriangleUV TriUVs[];
};

// Emettitori per la NEE (light sampling + MIS). Vuoto (numLights == 0) su tutte le scene
// senza geometria emissiva -> la NEE si disattiva del tutto: nessun costo, render identico.
layout(std430, binding = 17) restrict readonly buffer LightsBuffer {
    Light Lights[];
};

layout(binding = 18) uniform accelerationStructureEXT topLevel;

// I normal map sono dati, non colore: un array a parte in formato UNORM, senza la decodifica
// sRGB che l'array dell'albedo applica in hardware.
layout(binding = 19) uniform sampler2DArray DataTextures; // normal map e alpha mask MTL
