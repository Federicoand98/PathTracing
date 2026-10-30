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

        // Per le mesh insideHit dice solo che la faccia e' vista da dietro. Un interno ce l'ha
        // solo un materiale rifrattivo: una mesh aperta e opaca (un muro, una tenda) vista da
        // dietro e' una superficie normale, non un mezzo che assorbe. Chi usa Inside ribalta
        // la normale, quindi qui va lasciata concorde col raggio.
        bool inside = insideHit && Materials[clamp(triMaterial, 0, numMaterials - 1)].RefractionProbability > 0.0;
        if(inside)
            normal = -normal;

        hit = HitInfo(hitDistance, hitPosition, normal, meshIndex, 2, inside, uv, triMaterial);
    }

    return hit;
}

// Le primitive analitiche stanno in un BLAS procedurale di AABB: il driver trova il box, ma
// l'intersezione vera resta qui. Ritorna la distanza del candidato, < 0 se non c'e'.
// L'indice del primitivo conta prima le sfere, poi i quad (vedi World::PrimitiveBounds).
float IntersectProcedural(Ray ray, int primitive, float tMin) {
    if(primitive < numSpheres) {
        Sphere sphere = Spheres[primitive];
        float t1, t2;
        if(!HitSphere(ray, sphere, t1, t2))
            return -1.0;
        return t1 >= tMin ? t1 : (t2 >= tMin ? t2 : -1.0);
    }
    Quad quad = Quads[primitive - numSpheres];
    float t = HitQuad(ray, quad);
    return t >= tMin ? t : -1.0;
}

// Promuove un candidato procedurale a intersezione, se e' davanti a cio' che c'e' gia'.
void CommitProcedural(rayQueryEXT query, Ray ray, float tMin, float tMax) {
    float t = IntersectProcedural(ray, rayQueryGetIntersectionPrimitiveIndexEXT(query, false), tMin);
    if(rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        tMax = rayQueryGetIntersectionTEXT(query, true);
    if(t >= tMin && t <= tMax)
        rayQueryGenerateIntersectionEXT(query, t);
}

HitInfo TraceRay(Ray ray) {
    const float tMax = 100000.0;
    rayQueryEXT query;
    rayQueryInitializeEXT(query, topLevel, gl_RayFlagsOpaqueEXT,
                          0xFF, ray.Origin, 0.0, ray.Direction, tMax);
    while(rayQueryProceedEXT(query))
        if(rayQueryGetIntersectionTypeEXT(query, false) == gl_RayQueryCandidateIntersectionAABBEXT)
            CommitProcedural(query, ray, 1e-4, tMax);

    uint type = rayQueryGetIntersectionTypeEXT(query, true);
    if(type == gl_RayQueryCommittedIntersectionNoneEXT)
        return NoHit();

    float t = rayQueryGetIntersectionTEXT(query, true);
    int primitive = rayQueryGetIntersectionPrimitiveIndexEXT(query, true);

    if(type == gl_RayQueryCommittedIntersectionTriangleEXT) {
        int mesh = rayQueryGetIntersectionInstanceCustomIndexEXT(query, true);
        vec2 barycentrics = rayQueryGetIntersectionBarycentricsEXT(query, true);
        bool backFace = !rayQueryGetIntersectionFrontFaceEXT(query, true);
        return HandleHit(ray, t, int(Meshes[mesh].FirstTriangle) + primitive, 2, backFace, barycentrics.x, barycentrics.y, mesh);
    }

    if(primitive < numSpheres) {
        Sphere sphere = Spheres[primitive];
        float t1, t2;
        HitSphere(ray, sphere, t1, t2);
        return HandleHit(ray, t, primitive, 0, t == t2, 0.0, 0.0, -1);
    }
    return HandleHit(ray, t, primitive - numSpheres, 1, false, 0.0, 0.0, -1);
}

// Shadow ray: c'e' un occlusore tra origin e la luce (entro maxDist)? Si ferma al primo.
bool Occluded(vec3 origin, vec3 dir, float maxDist) {
    Ray ray = Ray(origin, dir, 0.0);
    rayQueryEXT query;
    rayQueryInitializeEXT(query, topLevel,
                          gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT,
                          0xFF, origin, EPSILON, dir, maxDist);
    while(rayQueryProceedEXT(query))
        if(rayQueryGetIntersectionTypeEXT(query, false) == gl_RayQueryCandidateIntersectionAABBEXT)
            CommitProcedural(query, ray, EPSILON, maxDist);

    return rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT;
}
