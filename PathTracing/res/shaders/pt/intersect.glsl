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
