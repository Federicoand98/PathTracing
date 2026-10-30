#ifndef COMMON_CONSTANTS_GLSL
#define COMMON_CONSTANTS_GLSL

#define EPSILON 0.001
#define PI              3.1415926

// Floor della demodulazione, condiviso tra path tracer e denoiser: demodulare e rimodulare
// con lo stesso valore clampato e' cio' che rende il giro esattamente invertibile (con
// l'albedo grezzo al ritorno i materiali scuri perderebbero energia in modo permanente).
const float ALBEDO_FLOOR = 0.05;

float Luminance(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

#endif
