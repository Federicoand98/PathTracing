#version 430 core
out vec4 FragColor;
	
in vec2 TexCoords;
	
uniform sampler2D tex;
uniform bool PostProcessing;

vec3 LinearToInverseGamma(vec3 rgb, float gamma);
vec3 ACESFilm(vec3 x);
	
void main() {             
    vec3 texCol = texture(tex, TexCoords).rgb;      
    
    if(PostProcessing) {
        texCol = ACESFilm(texCol);
        texCol = LinearToInverseGamma(texCol, 2.4);
    }

    //FragColor = vec4(texCol, 1.0);
    FragColor = vec4(1.0);
}

vec3 LinearToInverseGamma(vec3 rgb, float gamma) {
    return mix(pow(rgb, vec3(1.0 / gamma)) * 1.055 - 0.055, rgb * 12.92, vec3(lessThan(rgb, 0.0031308.xxx)));
}

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
