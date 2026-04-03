#version 410 core

in vec2 vTexCoord;
out vec2 fragColor;

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
    return normalize(tangent * h.x + bitangent * h.y + n * h.z);
}

float geometrySchlickGGX(float ndotv, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0;
    return ndotv / (ndotv * (1.0 - k) + k);
}

float geometrySmith(float roughness, float ndotv, float ndotl) {
    return geometrySchlickGGX(ndotv, roughness) * geometrySchlickGGX(ndotl, roughness);
}

vec2 IntegrateBRDF(float ndotv, float roughness) {
    vec3 v;
    v.x = sqrt(max(1.0 - ndotv * ndotv, 0.0));
    v.y = 0.0;
    v.z = ndotv;

    float a = 0.0;
    float b = 0.0;
    vec3 n = vec3(0.0, 0.0, 1.0);

    const uint sampleCount = 64u;
    for (uint i = 0u; i < sampleCount; ++i) {
        vec2 xi = hammersley(i, sampleCount);
        vec3 h = importanceSampleGGX(xi, n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);

        float ndotl = max(l.z, 0.0);
        float ndoth = max(h.z, 0.0);
        float vdoth = max(dot(v, h), 0.0);
        if (ndotl > 0.0) {
            float g = geometrySmith(roughness, ndotv, ndotl);
            float gVis = (g * vdoth) / max(ndoth * ndotv, 0.0001);
            float fc = pow(1.0 - vdoth, 5.0);

            a += (1.0 - fc) * gVis;
            b += fc * gVis;
        }
    }

    return vec2(a, b) / float(sampleCount);
}

void main() {
    fragColor = IntegrateBRDF(vTexCoord.x, vTexCoord.y);
}
