#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 fragColor;

struct PointLight {
    vec3 position;
    vec3 color;
    float radius;
    float intensity;
};

uniform PointLight uPointLights[8];
uniform int uNumLights;
uniform vec3 uCameraPos;

// Smooth quartic attenuation -- avoids hard ring artifacts in dithered output (Pitfall 6)
float attenuation(float dist, float radius) {
    float x = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    return x * x;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);

    float ambient = 0.08;  // low ambient for dark gothic feel (D-04: balanced, not dark-heavy)
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

    // Output in linear space -- dither pass reads this directly (D-03)
    fragColor = vec4(vec3(clamp(totalLight, 0.0, 1.0)), 1.0);
}
