// contatore di nodi BVH visitati (test AABB) usato dal debugger BVH
int gNodeVisits = 0;

// Parti comuni di hitBVH e TracePrimBVH, che differiscono solo per il contenuto delle foglie.

// Slab test dei 4 figli in parallelo (SoA -> vec4). Definisce tEnter e tExit.
#define BVH_SLAB_TEST(node) \
    vec4 tminx = (node.bminx - ray.Origin.x) * invDir.x; \
    vec4 tmaxx = (node.bmaxx - ray.Origin.x) * invDir.x; \
    vec4 tminy = (node.bminy - ray.Origin.y) * invDir.y; \
    vec4 tmaxy = (node.bmaxy - ray.Origin.y) * invDir.y; \
    vec4 tminz = (node.bminz - ray.Origin.z) * invDir.z; \
    vec4 tmaxz = (node.bmaxz - ray.Origin.z) * invDir.z; \
    vec4 tEnter = max(max(min(tminx, tmaxx), min(tminy, tmaxy)), max(min(tminz, tmaxz), vec4(0.0))); \
    vec4 tExit  = min(min(max(tminx, tmaxx), max(tminy, tmaxy)), max(tminz, tmaxz));

// Figli interni colpiti (t = FLOAT_MAX se non validi). ray.t e' gia' quello aggiornato dalle
// foglie, quindi il test esclude i rami dietro. Rete di sort a 4 elementi (5 compare-swap),
// poi push dal piu' lontano al piu' vicino: il piu' vicino finisce in cima allo stack.
#define BVH_SORT_AND_PUSH() \
    float ft; int fi; \
    if(ct0 > ct1) { ft=ct0; ct0=ct1; ct1=ft; fi=ci0; ci0=ci1; ci1=fi; } \
    if(ct2 > ct3) { ft=ct2; ct2=ct3; ct3=ft; fi=ci2; ci2=ci3; ci3=fi; } \
    if(ct0 > ct2) { ft=ct0; ct0=ct2; ct2=ft; fi=ci0; ci0=ci2; ci2=fi; } \
    if(ct1 > ct3) { ft=ct1; ct1=ct3; ct3=ft; fi=ci1; ci1=ci3; ci3=fi; } \
    if(ct1 > ct2) { ft=ct1; ct1=ct2; ct2=ft; fi=ci1; ci1=ci2; ci2=fi; } \
    if(ct3 < FLOAT_MAX && stackPtr < MAX_STACK) stack[stackPtr++] = ci3; \
    if(ct2 < FLOAT_MAX && stackPtr < MAX_STACK) stack[stackPtr++] = ci2; \
    if(ct1 < FLOAT_MAX && stackPtr < MAX_STACK) stack[stackPtr++] = ci1; \
    if(ct0 < FLOAT_MAX && stackPtr < MAX_STACK) stack[stackPtr++] = ci0;

#define BVH_PUSH_CHILDREN() \
    int   ci0 = node.child.x, ci1 = node.child.y, ci2 = node.child.z, ci3 = node.child.w; \
    float ct0 = (node.count.x == 0 && tEnter.x <= tExit.x && tEnter.x < ray.t) ? tEnter.x : FLOAT_MAX; \
    float ct1 = (node.count.y == 0 && tEnter.y <= tExit.y && tEnter.y < ray.t) ? tEnter.y : FLOAT_MAX; \
    float ct2 = (node.count.z == 0 && tEnter.z <= tExit.z && tEnter.z < ray.t) ? tEnter.z : FLOAT_MAX; \
    float ct3 = (node.count.w == 0 && tEnter.w <= tExit.w && tEnter.w < ray.t) ? tEnter.w : FLOAT_MAX; \
    BVH_SORT_AND_PUSH()

// Traversata del BVH a 4 vie. Per ogni nodo si testano i 4 box figli in parallelo
// (op vec4 sul layout SoA), poi si intersecano subito le foglie colpite (restringe
// ray.t) e si impilano i figli interni dal piu' lontano al piu' vicino, cosi' il piu'
// vicino sta in cima allo stack ed e' esplorato per primo (front-to-back).
float hitBVH(inout Ray ray, int rootNode, out int hitTriIndex, out float outU, out float outV, bool anyHit) {
    // Lo stack e' fisso: il BVH4 impila fino a 3 figli interni per nodo, quindi puo'
    // servire piu' spazio della sola profondita'. Deve restare allineato a SHADER_MAX_STACK
    // in World::BuildBVH(). I push oltre il limite vengono scartati (niente overflow).
    const int MAX_STACK = 64;
    int stack[MAX_STACK];
    int stackPtr = 0;

    hitTriIndex = -1;
    outU = 0.0;
    outV = 0.0;

    vec3 invDir = 1.0 / ray.Direction;
    stack[stackPtr++] = rootNode;

    while(stackPtr > 0) {
        BVH4Node node = BVH4Nodes[stack[--stackPtr]];
        gNodeVisits++;

        BVH_SLAB_TEST(node)

        // Da qui i 4 figli si trattano SROTOLATI con swizzle statico (.x/.y/.z/.w).
        // Niente indicizzazione dinamica dei vettori (node.count[c], tEnter[c], ...): sulla
        // GPU costringerebbe il compilatore a spillare i vettori in memoria locale -> lento.
        // Un figlio e' colpito se: box centrato (tEnter<=tExit) e davanti al hit (tEnter<ray.t);
        // count>0 => foglia, count==0 => interno, count<0 => slot vuoto (mai colpito).

        // FOGLIE: intersecale subito, cosi' restringono ray.t per i figli interni.
        // (LANE = componente del nodo: non uso 'C' perche' collide con tri.C del triangolo)
        #define LEAF(LANE) if(node.count.LANE > 0 && tEnter.LANE <= tExit.LANE && tEnter.LANE < ray.t) { \
            int firstTri = node.child.LANE; \
            for(int k = 0; k < node.count.LANE; k++) { \
                int triIndex = TriIndex[firstTri + k]; \
                TrianglePosition tri = TriPositions[triIndex]; \
                float u, v; \
                float d = HitTriangle(ray, vec3(tri.A), vec3(tri.B), vec3(tri.C), u, v); \
                if(d >= 0.0 && d < ray.t) { \
                    ray.t = d; outU = u; outV = v; hitTriIndex = triIndex; \
                    if(anyHit && d > EPSILON) return d; \
                } \
            } }
        LEAF(x) LEAF(y) LEAF(z) LEAF(w)
        #undef LEAF

        BVH_PUSH_CHILDREN()
    }

    return hitTriIndex >= 0 ? ray.t : -1.0;
}

#if USE_PRIM_BVH
// Traversata del BVH costruito su sfere e quad. Stessa struttura di hitBVH (slab test dei 4
// figli in vec4, foglie intersecate subito per restringere ray.t, interni impilati dal piu'
// lontano al piu' vicino), ma le foglie contengono primitive ETEROGENEE: il codice in
// PrimIndex porta il tipo nel bit basso e l'indice nel resto.
// Le primitive stanno in world space, quindi qui non c'e' nessuna trasformazione del raggio.
bool TracePrimBVH(inout Ray ray, out int outType, out int outIndex, out bool outInside, bool anyHit) {
    outType = -1;
    outIndex = -1;
    outInside = false;
    if(primBvhRoot < 0)
        return false;

    // Stack DELIBERATAMENTE piccolo: questo BVH copre poche centinaia di primitive, quindi e'
    // basso. Ogni entry e' memoria locale per-thread e si somma a quella di hitBVH: portarlo a
    // 64 come li' raddoppiava lo spilling e faceva crollare l'occupancy (misurato).
    const int MAX_STACK = 24;
    int stack[MAX_STACK];
    int stackPtr = 0;
    bool found = false;

    vec3 invDir = 1.0 / ray.Direction;
    stack[stackPtr++] = primBvhRoot;

    while(stackPtr > 0) {
        BVH4Node node = BVH4Nodes[stack[--stackPtr]];
        gNodeVisits++;

        BVH_SLAB_TEST(node)

        // FOGLIE: intersecale subito, cosi' restringono ray.t per i figli interni.
        #define PLEAF(LANE) if(node.count.LANE > 0 && tEnter.LANE <= tExit.LANE && tEnter.LANE < ray.t) { \
            int first = node.child.LANE; \
            for(int k = 0; k < node.count.LANE; k++) { \
                int code = PrimIndex[first + k]; \
                int pidx = code >> 1; \
                if((code & 1) == 0) { \
                    float s1, s2; \
                    Sphere sphere = Spheres[pidx]; \
                    if(HitSphere(ray, sphere, s1, s2) && s2 > 0.0) { \
                        float d = (s1 < 0.0) ? s2 : s1; \
                        if(d > 0.0 && d < ray.t) { \
                            ray.t = d; outType = 0; outIndex = pidx; outInside = (d == s2); found = true; \
                            if(anyHit) return true; \
                        } \
                    } \
                } else { \
                    Quad quad = Quads[pidx]; \
                    float d = HitQuad(ray, quad); \
                    if(d >= 0.0 && d < ray.t) { \
                        ray.t = d; outType = 1; outIndex = pidx; outInside = false; found = true; \
                        if(anyHit) return true; \
                    } \
                } \
            } }
        PLEAF(x) PLEAF(y) PLEAF(z) PLEAF(w)
        #undef PLEAF

        BVH_PUSH_CHILDREN()
    }

    return found;
}
#endif

// BVH a due livelli. Invece di trasformare i triangoli nel mondo (che imporrebbe di
// ribakarli e ricostruire il BVH ad ogni spostamento), si trasforma il RAGGIO nello
// spazio locale della mesh. La direzione locale NON viene rinormalizzata: cosi' il
// parametro t resta lo stesso in entrambi gli spazi ed e' direttamente confrontabile.
void IntersectMesh(int m, inout Ray ray, vec3 invDirWorld, bool anyHit,
                   inout float closest, inout int hitTriIndex, inout int hitMesh, inout float outU, inout float outV) {
    MeshInfo mesh = Meshes[m];
    if(int(mesh.NumTriangles) == 0)
        return;

    // reject rapido contro l'AABB della mesh in world space
    if(hitAABB(ray, invDirWorld, vec3(mesh.BoundsMin), vec3(mesh.BoundsMax)) == FLOAT_MAX)
        return;

    Ray local;
    local.Origin = vec3(mesh.InvTransform * vec4(ray.Origin, 1.0));
    local.Direction = vec3(mesh.InvTransform * vec4(ray.Direction, 0.0));
    local.t = ray.t;

    int triIndex;
    float u, v;
    float distance = hitBVH(local, int(mesh.RootNode), triIndex, u, v, anyHit);

    if(distance >= 0.0 && distance < ray.t) {
        ray.t = distance; // restringe la ricerca per le mesh successive
        closest = distance;
        hitTriIndex = triIndex;
        hitMesh = m;
        outU = u;
        outV = v;
    }
}

float TraceMeshes(inout Ray ray, out int hitTriIndex, out int hitMesh, out float outU, out float outV, bool anyHit) {
    hitTriIndex = -1;
    hitMesh = -1;
    outU = 0.0;
    outV = 0.0;

    if(BVH4Nodes.length() == 0)
        return -1.0;

    vec3 invDir = 1.0 / ray.Direction;
    float closest = -1.0;

#if USE_TLAS
    // Le foglie del TLAS entrano nello stack e nell'ordinamento come i nodi interni, codificate
    // come ~((nodo << 2) | lane). Cosi' c'e' un solo punto che scende in un BLAS, quindi un solo
    // hitBVH inlinato, e le mesh vicine vengono visitate per prime restringendo ray.t.
    const int MAX_STACK = 32;
    int stack[MAX_STACK];
    int stackPtr = 0;
    stack[stackPtr++] = tlasRoot;

    while(stackPtr > 0) {
        int entry = stack[--stackPtr];

        if(entry < 0) {
            int leaf = ~entry;
            int nodeIndex = leaf >> 2, lane = leaf & 3;
            int first = BVH4Nodes[nodeIndex].child[lane];
            int count = BVH4Nodes[nodeIndex].count[lane];
            for(int k = 0; k < count; k++) {
                IntersectMesh(PrimIndex[first + k], ray, invDir, anyHit, closest, hitTriIndex, hitMesh, outU, outV);
                if(anyHit && closest > EPSILON)
                    return closest;
            }
            continue;
        }

        BVH4Node node = BVH4Nodes[entry];
        gNodeVisits++;

        BVH_SLAB_TEST(node)

        int ci0 = node.count.x > 0 ? ~(entry << 2) : node.child.x;
        int ci1 = node.count.y > 0 ? ~((entry << 2) | 1) : node.child.y;
        int ci2 = node.count.z > 0 ? ~((entry << 2) | 2) : node.child.z;
        int ci3 = node.count.w > 0 ? ~((entry << 2) | 3) : node.child.w;
        float ct0 = (node.count.x >= 0 && tEnter.x <= tExit.x && tEnter.x < ray.t) ? tEnter.x : FLOAT_MAX;
        float ct1 = (node.count.y >= 0 && tEnter.y <= tExit.y && tEnter.y < ray.t) ? tEnter.y : FLOAT_MAX;
        float ct2 = (node.count.z >= 0 && tEnter.z <= tExit.z && tEnter.z < ray.t) ? tEnter.z : FLOAT_MAX;
        float ct3 = (node.count.w >= 0 && tEnter.w <= tExit.w && tEnter.w < ray.t) ? tEnter.w : FLOAT_MAX;
        BVH_SORT_AND_PUSH()
    }
#else
    for(int m = 0; m < Meshes.length(); m++) {
        IntersectMesh(m, ray, invDir, anyHit, closest, hitTriIndex, hitMesh, outU, outV);
        if(anyHit && closest > EPSILON)
            return closest;
    }
#endif

    return closest;
}
