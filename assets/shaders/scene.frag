#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vLocalPos;

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
uniform vec3 uBaseColor;
uniform int uMaterialKind;

const int MATERIAL_STONE = 0;
const int MATERIAL_WOOD  = 1;
const int MATERIAL_METAL = 2;
const int MATERIAL_WAX   = 3;
const int MATERIAL_MOSS  = 4;
const int MATERIAL_VIEWMODEL = 5;

// Smooth quartic attenuation
float attenuation(float dist, float radius) {
    float x = clamp(1.0 - pow(dist / radius, 4.0), 0.0, 1.0);
    return x * x;
}

float hash13(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z);
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        p = p * 2.03 + vec3(19.1, 7.7, 13.4);
        amplitude *= 0.5;
    }
    return value;
}

vec3 detailStone(vec3 baseColor, vec3 N) {
    float macro = fbm(vWorldPos * vec3(0.28, 0.20, 0.28));
    float micro = fbm(vWorldPos * 1.9 + N * 0.8);
    float heightWear = clamp((vWorldPos.y + 0.5) * 0.18, 0.0, 1.0);
    float dampMask = smoothstep(1.2, -0.3, vWorldPos.y) * smoothstep(0.45, 0.82, macro);

    vec3 shadowStone = baseColor * vec3(0.78, 0.78, 0.78);
    vec3 warmStone = baseColor * vec3(1.06, 1.06, 1.06);
    vec3 detail = mix(shadowStone, warmStone, macro * 0.65 + heightWear * 0.20);
    detail *= 0.90 + micro * 0.15;
    detail = mix(detail, baseColor * vec3(0.58, 0.58, 0.58), dampMask * 0.10);
    return detail;
}

vec3 detailWood(vec3 baseColor) {
    float plankBands = 0.5 + 0.5 * sin((vLocalPos.x + 0.1) * 7.0);
    float grain = fbm(vec3(vLocalPos.y * 10.0, vLocalPos.z * 5.0, vLocalPos.x * 3.0) + vWorldPos * 0.35);
    float knots = fbm(vec3(vLocalPos.z * 8.0, vLocalPos.y * 3.0, vLocalPos.x * 8.0));

    vec3 darkWood = baseColor * vec3(0.70, 0.70, 0.70);
    vec3 lightWood = baseColor * vec3(1.08, 1.08, 1.08);
    vec3 detail = mix(darkWood, lightWood, grain);
    detail *= 0.90 + 0.14 * plankBands;
    detail = mix(detail, detail * vec3(0.62, 0.62, 0.62), smoothstep(0.60, 0.85, knots) * 0.24);
    return detail;
}

vec3 detailMetal(vec3 baseColor, vec3 N) {
    float brushed = fbm(vec3(vLocalPos.y * 18.0, vLocalPos.x * 4.0, vLocalPos.z * 4.0));
    float tarnish = fbm(vWorldPos * 2.4 + vLocalPos * 6.0);
    float ridgeSheen = pow(max(dot(N, normalize(vec3(0.3, 0.8, 0.4))), 0.0), 3.0);
    vec3 darkMetal = baseColor * vec3(0.72, 0.72, 0.72);
    vec3 brightMetal = baseColor * vec3(1.08, 1.08, 1.08);
    vec3 detail = mix(darkMetal, brightMetal, brushed);
    vec3 tarnishColor = baseColor * vec3(0.52, 0.52, 0.52);
    detail = mix(detail, tarnishColor, smoothstep(0.72, 0.92, tarnish) * 0.08);
    detail *= 0.94 + ridgeSheen * 0.14;
    return detail;
}

vec3 detailWax(vec3 baseColor, vec3 N) {
    float drips = fbm(vec3(vLocalPos.x * 7.0, vLocalPos.y * 14.0, vLocalPos.z * 7.0));
    vec3 softWax = baseColor * vec3(0.90, 0.90, 0.90);
    vec3 warmWax = baseColor * vec3(1.06, 1.06, 1.06);
    vec3 detail = mix(softWax, warmWax, drips);
    detail *= 0.96 + 0.08 * max(N.y, 0.0);
    return detail;
}

vec3 detailMoss(vec3 baseColor) {
    float clump = fbm(vWorldPos * 2.6 + vLocalPos * 2.0);
    float damp = fbm(vWorldPos * 0.9 + vec3(12.4, 5.7, 8.1));
    vec3 darkMoss = baseColor * vec3(0.78, 0.78, 0.78);
    vec3 brightMoss = baseColor * vec3(1.00, 1.00, 1.00);
    vec3 detail = mix(darkMoss, brightMoss, clump);
    detail = mix(detail, baseColor * vec3(0.62, 0.62, 0.62), smoothstep(0.72, 0.90, damp) * 0.14);
    return detail;
}

vec3 detailSkin(vec3 baseColor, vec3 N) {
    float tone = fbm(vec3(vLocalPos.x * 0.06, vLocalPos.y * 0.05, vLocalPos.z * 0.08));
    vec3 shadowSkin = baseColor * vec3(0.88, 0.88, 0.88);
    vec3 warmSkin = baseColor * vec3(1.06, 1.06, 1.06);
    vec3 detail = mix(shadowSkin, warmSkin, tone);
    detail *= 0.96 + 0.08 * max(N.y, 0.0);
    return detail;
}

vec3 detailViewmodel(vec3 baseColor, vec3 N) {
    float inX = step(-171.5, vLocalPos.x) * (1.0 - step(-141.0, vLocalPos.x));
    float inY = step(-356.5, vLocalPos.y) * (1.0 - step(-270.0, vLocalPos.y));
    float inZ = step(0.0, vLocalPos.z) * (1.0 - step(7.2, vLocalPos.z));
    float daggerMask = inX * inY * inZ;

    float bladeMask = daggerMask * smoothstep(-309.0, -295.0, vLocalPos.y);
    float handleMask = daggerMask * (1.0 - smoothstep(-336.0, -322.0, vLocalPos.y));
    float guardMask = clamp(daggerMask - bladeMask - handleMask, 0.0, 1.0);

    vec3 skin = detailSkin(baseColor, N);
    vec3 handle = detailWood(vec3(0.22, 0.22, 0.22));
    vec3 guard = detailMetal(vec3(0.54, 0.54, 0.54), N);
    vec3 blade = detailMetal(vec3(0.74, 0.74, 0.74), N);

    vec3 dagger = handle * handleMask + guard * guardMask + blade * bladeMask;
    return mix(skin, dagger, daggerMask);
}

vec3 applyMaterialDetail(vec3 baseColor, vec3 N) {
    if (uMaterialKind == MATERIAL_WOOD) {
        return detailWood(baseColor);
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return detailMetal(baseColor, N);
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return detailWax(baseColor, N);
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return detailMoss(baseColor);
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return detailViewmodel(baseColor, N);
    }
    return detailStone(baseColor, N);
}

float materialSpecStrength() {
    if (uMaterialKind == MATERIAL_WOOD) {
        return 0.07;
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return 0.36;
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return 0.18;
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return 0.03;
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return 0.22;
    }
    return 0.10;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 baseColor = clamp(uBaseColor, 0.0, 1.0);
    vec3 albedo = applyMaterialDetail(baseColor, N);
    float specStrength = materialSpecStrength();

    vec3 ambientLight = vec3(0.054, 0.051, 0.047);
    ambientLight += vec3(0.11, 0.10, 0.09) * max(N.y, 0.0);
    ambientLight += vec3(0.028, 0.026, 0.024) * max(-N.y, 0.0);
    vec3 totalLight = albedo * ambientLight;

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
        float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);

        vec3 lightColor = uPointLights[i].color * atten * uPointLights[i].intensity;
        totalLight += albedo * lightColor * (diff * 0.92);
        totalLight += mix(albedo, lightColor, 0.55) * (spec * specStrength + fresnel * specStrength * 0.25);
    }

    fragColor = vec4(clamp(totalLight, 0.0, 1.0), 1.0);

    // Output world-space normal (packed to 0..1 range for edge detection)
    fragNormal = vec4(N * 0.5 + 0.5, 1.0);
}
