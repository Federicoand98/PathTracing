// Mappatura sferica equirettangolare: la sfera non ha UV proprie, si derivano dalla normale.
// Mappatura equirettangolare. Le texture sono caricate con flip verticale, quindi v = 0
// e' la riga BASSA dell'immagine: il polo sud deve finire a v = 0, non il nord.
// Con una scacchiera, simmetrica, l'errore non si vedeva.
vec2 SphereUV(vec3 normal) {
    return vec2(0.5 + atan(normal.z, normal.x) / (2.0 * PI),
                0.5 + asin(clamp(normal.y, -1.0, 1.0)) / PI);
}

bool HitSphere(Ray ray, Sphere sphere, out float t1, out float t2) {
    vec3 spherePos = vec3(sphere.Position);
    float radius = sphere.Position.w;

    vec3 oc = ray.Origin - spherePos;
    float a = dot(ray.Direction, ray.Direction);
    float b = 2.0 * dot(oc, ray.Direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;

    if(discriminant < 0.0)
        return false;

    t1 = (-b - sqrt(discriminant)) / (2.0 * a);
    t2 = (-b + sqrt(discriminant)) / (2.0 * a);

    return true;
}

float HitQuad(Ray ray, Quad quad) {
    vec3 normal = normalize(cross(vec3(quad.U), vec3(quad.V)));
    float d = dot(normal, vec3(quad.PositionLLC));
    vec3 w = normal / (dot(normal, normal) + 0.0001);

    float denom = dot(normal, ray.Direction);
    
    // no hit if the ray is parallel to the plane
    if (abs(denom) < 1e-8)
        return -1.0;

    float t = (d - dot(normal, ray.Origin)) / denom;
    if (t < 0.001)
        return -1.0;

    vec3 intersection = ray.Origin + (t * ray.Direction);
    vec3 planar_hitpt_vector = intersection - vec3(quad.PositionLLC);
    float alpha = dot(w, cross(planar_hitpt_vector, vec3(quad.V)));
    float beta = dot(w, cross(vec3(quad.U), planar_hitpt_vector));

    // is not interior
    if ((alpha < 0) || (quad.Width < alpha) || (beta < 0) || (quad.Height < beta))
        return -1;

    return t;
}

float HitTriangle(Ray ray, vec3 A, vec3 B, vec3 C, out float u, out float v) {
    vec3 edgeAB = vec3(B) - vec3(A);
    vec3 edgeAC = vec3(C) - vec3(A);
    vec3 normalVector = cross(edgeAB, edgeAC);
    vec3 ao = ray.Origin - vec3(A);
    vec3 dao = cross(ao, ray.Direction);

    float determinant = -dot(ray.Direction, normalVector);
    float invDet = 1 / determinant;
    
    // Calculate dst to triangle & barycentric coordinates of intersection point
    float dst = dot(ao, normalVector) * invDet;
    u = dot(edgeAC, dao) * invDet;
    v = -dot(edgeAB, dao) * invDet;
    float w = 1 - u - v;

    if(determinant >= 1E-6 && dst >= 0 && u >= 0 && v >= 0 && w >= 0)
        return dst;
    
    return -1.0;
}

bool HitBoundingBox(Ray ray, vec3 boxMin, vec3 boxMax) {
    vec3 invDir = 1 / ray.Direction;
    vec3 tMin = (boxMin - ray.Origin) * invDir;
    vec3 tMax = (boxMax - ray.Origin) * invDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);

    return tNear <= tFar;
}

// Ritorna la distanza d'ingresso nella AABB, oppure FLOAT_MAX se il raggio la manca
// (o se la colpisce oltre l'hit piu' vicino gia' trovato, ray.t).
// invDir e' precalcolato una volta per raggio: sulle GPU la divisione costa
// molto piu' della moltiplicazione, e qui la si pagherebbe 6 volte per nodo.
float hitAABB(Ray ray, vec3 invDir, vec3 bmin, vec3 bmax) {
    vec3 t1 = (bmin - ray.Origin) * invDir;
    vec3 t2 = (bmax - ray.Origin) * invDir;
    vec3 tsmall = min(t1, t2);
    vec3 tbig = max(t1, t2);

    float tEnter = max(max(tsmall.x, tsmall.y), max(tsmall.z, 0.0));
    float tExit = min(min(tbig.x, tbig.y), tbig.z);

    return (tExit >= tEnter && tEnter < ray.t) ? tEnter : FLOAT_MAX;
}
