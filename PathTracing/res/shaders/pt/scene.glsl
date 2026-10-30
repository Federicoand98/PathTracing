Ray CalculateRayFromCamera(mat4 inverseProj, mat4 inverseView, vec3 cameraPos, vec2 ndc) {
    vec4 target = inverseProj * vec4(ndc, 1.0, 1.0);
    vec3 rayDirection = vec3(inverseView * vec4(normalize(vec3(target) / target.w), 0.0));

    return Ray(cameraPos, rayDirection, 0);
}

HitInfo NoHit() {
    return HitInfo(-1.0, vec3(0.0), vec3(0.0), -1, -1, false, vec2(0.0), -1);
}

HitInfo HandleHit(Ray ray, float hitDistance, int index, int objType, bool insideHit, float u, float v, int meshIndex) {
    HitInfo hit;

    if(objType == 0) { // SPHERE
        vec3 spherePos = vec3(Spheres[index].Position);

        vec3 origin = ray.Origin - spherePos;
        vec3 closestPosition = spherePos;
        vec3 hitPosition = origin + ray.Direction * hitDistance;
        vec3 normal = normalize(hitPosition);
        hitPosition += closestPosition;

        hit = HitInfo(hitDistance, hitPosition, normal, index, 0, insideHit, SphereUV(normal), int(Spheres[index].Mat));
    }
    else if(objType == 1) { // QUAD
        Quad quad = Quads[index];
        vec3 origin = ray.Origin - vec3(quad.PositionLLC);
        vec3 closestPosition = vec3(quad.PositionLLC);
        vec3 hitPosition = origin + ray.Direction * hitDistance;
        vec3 normal = normalize(cross(vec3(quad.U), vec3(quad.V)));

        // il quad si estende per Width lungo U e Height lungo V: le coordinate affini
        // del punto d'impatto lungo quei due assi, normalizzate, sono gia' le UV
        vec3 planeNormal = cross(vec3(quad.U), vec3(quad.V));
        vec3 wVec = planeNormal / (dot(planeNormal, planeNormal) + 0.0001);
        float alpha = dot(wVec, cross(hitPosition, vec3(quad.V)));
        float beta = dot(wVec, cross(vec3(quad.U), hitPosition));
        vec2 uv = vec2(alpha / quad.Width, beta / quad.Height);

        hitPosition += closestPosition;

        hit = HitInfo(hitDistance, hitPosition, normal, index, 1, insideHit, uv, int(Quads[index].MaterialIndex));
    }
    else if(objType == 2) { // MESH
        // il raggio e' in world space e t e' lo stesso nei due spazi (direzione locale
        // non rinormalizzata), quindi il punto d'impatto si calcola direttamente qui
        vec3 hitPosition = ray.Origin + ray.Direction * hitDistance;

        // normale liscia interpolata coi pesi baricentrici (w->A, u->B, v->C).
        // Le normali si leggono solo qui, una volta, non nel loop caldo del BVH.
        TriangleNormal tn = TriNormals[index];
        float w = 1.0 - u - v;
        vec3 localNormal = vec3(tn.NormalA) * w
                         + vec3(tn.NormalB) * u
                         + vec3(tn.NormalC) * v;

        // le UV si interpolano coi medesimi pesi baricentrici delle normali
        TriangleUV tuv = TriUVs[index];
        vec2 uv = tuv.A.xy * w + tuv.B.xy * u + tuv.C.xy * v;

        // Materiale per-triangolo in tuv.A.z: < 0 = usa quello della mesh (comportamento
        // legacy delle mesh mono-materiale); >= 0 = OBJ multi-materiale (usemtl).
        int triMaterial = tuv.A.z >= 0.0 ? int(tuv.A.z) : int(Meshes[meshIndex].MaterialIndex);

        // local -> world con l'inversa trasposta: con scale non uniformi la normale
        // non si trasforma come una direzione, altrimenti smette di essere ortogonale
        mat3 normalMatrix = transpose(mat3(Meshes[meshIndex].InvTransform));
        vec3 normal = normalize(normalMatrix * localNormal);

        // orienta la normale contro il raggio: senza questo il BSDF riceve una
        // normale che punta "via" dal raggio (vn invertiti o silhouette) e produce
        // riflessioni sbagliate, self-intersection e macchie nere sulle mesh.
        if(dot(ray.Direction, normal) > 0.0)
            normal = -normal;

        hit = HitInfo(hitDistance, hitPosition, normal, meshIndex, 2, insideHit, uv, triMaterial);
    }

    return hit;
}

HitInfo TraceRay(Ray ray) {
    float hitDistance = 100000.0;
    int index = -1;
    int objType = -1; // 0 Sphere, 1 Quad, 2 Mesh
    float t1, t2;
    float u = 0, v = 0;
    int meshIndex = -1;
    bool inside;
    bool cubesPres = Cubes.length() > 0;

#if USE_PRIM_BVH
    // Sfere e quad passano dal loro BVH. Il ramo lineare sotto resta per la variante leggera
    // (poche primitive: scandirle costa meno che scendere un albero) e come fallback.
    if(primBvhRoot >= 0) {
        ray.t = hitDistance;
        int pType, pIndex; bool pInside;
        if(TracePrimBVH(ray, pType, pIndex, pInside, false)) {
            hitDistance = ray.t;
            index = pIndex;
            objType = pType;
            inside = pInside;
        }
    } else
#endif
    {
    for(int i = 0; i < Spheres.length(); i++) {
        Sphere sphere = Spheres[i];

        // La distanza va confrontata dopo aver scelto la radice: con l'origine dentro la sfera
        // t1 < 0 passerebbe sempre il test e t2 sovrascriverebbe un hit piu' vicino.
        if(HitSphere(ray, sphere, t1, t2) && t2 > 0.0) {
            float d = t1 < 0.0 ? t2 : t1;
            if(d < hitDistance) {
                hitDistance = d;
                index = i;
                inside = d == t2;
                objType = 0;
            }
        }
    }

    int j = 0;
    for(int i = 0; i < Quads.length(); i++) {
        if (cubesPres && j < Cubes.length() && Cubes[j].FirstQuad == i){
            bool hit = HitBoundingBox(ray, vec3(Cubes[j].BoundsMin), vec3(Cubes[j].BoundsMax));
            j++;
            if (!hit) {
                i+=5;
                continue;
            }
        }
        Quad quad = Quads[i];

        float distance = HitQuad(ray, quad);

        if(distance >= 0 && distance < hitDistance) {
            hitDistance = distance;
            index = i;
            objType = 1;
            inside = false;
        }
    }
    }

    if(BVH4Nodes.length() > 0) {
        // il test AABB usa ray.t per scartare i nodi oltre l'hit piu' vicino gia' trovato
        ray.t = hitDistance;

        int hitTriIndex, hitMesh;
        float outU, outV;
        float distance = TraceMeshes(ray, hitTriIndex, hitMesh, outU, outV, false);

        if(distance >= 0 && distance < hitDistance) {
            hitDistance = distance;
            index = hitTriIndex;
            meshIndex = hitMesh;
            objType = 2;
            inside = false;
            u = outU;
            v = outV;
        }
    }

    if(index < 0) {
        HitInfo result = NoHit();
        return result;
    }

    return HandleHit(ray, hitDistance, index, objType, inside, u, v, meshIndex);
}

// Shadow ray: c'e' un occlusore tra origin e la luce (entro maxDist)?
// E' un ANY-HIT: si ferma al PRIMO occlusore trovato, non cerca il piu' vicino. Non serve la
// superficie colpita, quindi l'early-out e' tutto il guadagno rispetto a TraceRay.
bool Occluded(vec3 origin, vec3 dir, float maxDist) {
#if USE_PRIM_BVH
    if(primBvhRoot >= 0) {
        Ray r = Ray(origin, dir, maxDist);
        int pType, pIndex; bool pInside;
        if(TracePrimBVH(r, pType, pIndex, pInside, true))
            return true;
        if(BVH4Nodes.length() > 0) {
            Ray rm = Ray(origin, dir, maxDist);
            int triIndex, meshIndex; float u, v;
            float d = TraceMeshes(rm, triIndex, meshIndex, u, v, true);
            if(d > EPSILON && d < maxDist)
                return true;
        }
        return false;
    }
#endif

    // --- sfere ---
    for(int i = 0; i < Spheres.length(); i++) {
        float t1, t2;
        Sphere sphere = Spheres[i];
        if(HitSphere(Ray(origin, dir, 0.0), sphere, t1, t2)) {
            float t = (t1 > EPSILON) ? t1 : t2;   // radice positiva piu' vicina
            if(t > EPSILON && t < maxDist)
                return true;
        }
    }

    // --- quad (stesso skip per box della TraceRay: un box e' 6 quad dietro una AABB) ---
    bool cubesPres = Cubes.length() > 0;
    int j = 0;
    for(int i = 0; i < Quads.length(); i++) {
        if(cubesPres && j < Cubes.length() && Cubes[j].FirstQuad == i) {
            bool hit = HitBoundingBox(Ray(origin, dir, 0.0), vec3(Cubes[j].BoundsMin), vec3(Cubes[j].BoundsMax));
            j++;
            if(!hit) {
                i += 5;
                continue;
            }
        }
        Quad quad = Quads[i];
        float t = HitQuad(Ray(origin, dir, 0.0), quad);
        if(t > EPSILON && t < maxDist)
            return true;
    }

    // --- mesh: limitando ray.t a maxDist la traversata pota tutto cio' che sta oltre la luce.
    if(BVH4Nodes.length() > 0) {
        Ray r = Ray(origin, dir, maxDist);
        int triIndex, meshIndex; float u, v;
        float d = TraceMeshes(r, triIndex, meshIndex, u, v, true);
        if(d > EPSILON && d < maxDist)
            return true;
    }

    return false;
}
