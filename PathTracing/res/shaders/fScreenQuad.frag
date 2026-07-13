#version 430 core
out vec4 FragColor;
	
in vec2 TexCoords;
	
uniform sampler2D tex;
uniform float Exposure;
uniform bool PostProcessing;

vec3 LessThan(vec3 f, float value);
vec3 LinearToSRGB(vec3 rgb, float gamma);
vec3 SRGBToLinear(vec3 rgb, float gamma);
vec3 ACESFilm(vec3 x);
	
void main() {
    // Pipeline unica: HDR lineare -> esposizione -> [ACES] -> encoding sRGB.
    vec3 color = texture(tex, TexCoords).rgb;   // colore lineare accumulato dal path tracer

    color *= Exposure;                          // esposizione: moltiplicatore lineare

    if(PostProcessing)
        color = ACESFilm(color);                // tonemap HDR -> [0,1] (clampa gia' lui)

    // Encoding sRGB SEMPRE: il framebuffer non e' sRGB e ImGui non converte, quindi senza
    // questo l'immagine uscirebbe scura. Senza tonemap i valori > 1 vengono troncati netti
    // da LinearToSRGB (clipping atteso: e' cosa vuol dire "niente tonemap").
    color = LinearToSRGB(color, 2.4);

    FragColor = vec4(color, 1.0);
}

vec3 LessThan(vec3 f, float value) {
    return vec3(
        (f.x < value) ? 1.0 : 0.0,
        (f.y < value) ? 1.0 : 0.0,
        (f.z < value) ? 1.0 : 0.0
    );
}

vec3 LinearToSRGB(vec3 rgb, float gamma) {
    rgb = clamp(rgb, 0.0, 1.0);
    return mix(pow(rgb, vec3(1.0 / gamma)) * 1.055 - 0.055, rgb * 12.92, LessThan(rgb, 0.0031308));
}

vec3 SRGBToLinear(vec3 rgb, float gamma) {
    rgb = clamp(rgb, 0.0, 1.0);
	return mix(pow(((rgb + 0.055f) / 1.055f), vec3(2.4f)), rgb / 12.92f, LessThan(rgb, 0.04045f));
}

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
