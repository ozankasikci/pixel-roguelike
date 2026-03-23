#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;

// MRT outputs: color + normals
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragNormal;

struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

uniform PointLight uPointLights[32];
uniform int uNumLights;
uniform vec3 uCameraPos;

// Smooth quartic attenuation
float attenuation(float dist, float radius) {
    float x = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    return x * x;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    float ambient = 0.15;
    float totalLight = ambient;

    for (int i = 0; i < uNumLights; i++) {
        vec3 L = uPointLights[i].position - vWorldPos;
        float dist = length(L);
        L = normalize(L);

        float atten = attenuation(dist, uPointLights[i].radius);

        // Diffuse (Lambertian)
        float diff = max(dot(N, L), 0.0);

        // Specular (Blinn-Phong)
        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);

        totalLight += (diff * 0.7 + spec * 0.3) * atten * uPointLights[i].intensity;
    }

    // Output luminance in linear space
    fragColor = vec4(vec3(clamp(totalLight, 0.0, 1.0)), 1.0);

    // Output world-space normal (packed to 0..1 range for edge detection)
    fragNormal = vec4(N * 0.5 + 0.5, 1.0);
}
