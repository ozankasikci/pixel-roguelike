#version 410 core

in vec3 vLocalDir;
out vec4 fragColor;

uniform samplerCube uEnvironmentMap;
uniform float uRoughness;

const float PI = 3.14159265359;

float radicalInverseVdc(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint n) {
    return vec2(float(i) / float(n), radicalInverseVdc(i));
}

vec3 importanceSampleGGX(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    vec3 h;
    h.x = cos(phi) * sinTheta;
    h.y = sin(phi) * sinTheta;
    h.z = cosTheta;

    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = cross(n, tangent);
    vec3 sampleVec = tangent * h.x + bitangent * h.y + n * h.z;
    return normalize(sampleVec);
}

void main() {
    vec3 n = normalize(vLocalDir);
    vec3 r = n;
    vec3 v = r;

    vec3 prefiltered = vec3(0.0);
    float totalWeight = 0.0;
    const uint sampleCount = 64u;
    for (uint i = 0u; i < sampleCount; ++i) {
        vec2 xi = hammersley(i, sampleCount);
        vec3 h = importanceSampleGGX(xi, n, uRoughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);
        float ndotl = max(dot(n, l), 0.0);
        if (ndotl > 0.0) {
            prefiltered += texture(uEnvironmentMap, normalize(vec3(l.x, l.y, -l.z))).rgb * ndotl;
            totalWeight += ndotl;
        }
    }

    prefiltered = prefiltered / max(totalWeight, 0.0001);
    fragColor = vec4(prefiltered, 1.0);
}
