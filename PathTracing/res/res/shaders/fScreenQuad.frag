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
    vec3 texCol = texture(tex, TexCoords).rgb;      
    const float gamma = 2.2;
    
    if(PostProcessing) {
		texCol *= 0.5;
        texCol = ACESFilm(texCol);
        texCol = LinearToSRGB(texCol, 2.4);
    }

    vec3 mapped = vec3(1.0) - exp(-texCol * Exposure);
    mapped = pow(mapped, vec3(1.0 / gamma));

    FragColor = vec4(texCol, 1.0);
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
