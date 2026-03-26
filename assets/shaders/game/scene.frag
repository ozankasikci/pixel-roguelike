#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vLocalPos;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragNormal;

struct RenderLight {
    int type;
    vec3 position;
    vec3 direction;
    vec3 color;
    float radius;
    float intensity;
    float innerConeCos;
    float outerConeCos;
    int castsShadows;
    int shadowIndex;
};

uniform RenderLight uLights[32];
uniform int uNumLights;
uniform sampler2D uShadowMaps[2];
uniform mat4 uShadowMatrices[2];
uniform int uShadowCount;
uniform int uEnableShadows;
uniform float uShadowBias;
uniform float uShadowNormalBias;
uniform vec3 uHemisphereSkyColor;
uniform vec3 uHemisphereGroundColor;
uniform float uHemisphereStrength;
uniform int uEnableDirectionalLights;
uniform float uDirectionalLightIntensityScale;
uniform vec3 uCameraPos;
uniform vec3 uBaseColor;
uniform int uMaterialKind;

const int LIGHT_POINT = 0;
const int LIGHT_SPOT = 1;
const int LIGHT_DIRECTIONAL = 2;

const int MATERIAL_STONE = 0;
const int MATERIAL_WOOD = 1;
const int MATERIAL_METAL = 2;
const int MATERIAL_WAX = 3;
const int MATERIAL_MOSS = 4;
const int MATERIAL_VIEWMODEL = 5;
const int MATERIAL_FLOOR = 6;

const float PI = 3.14159265359;

float attenuation(float dist, float radius) {
    float x = clamp(1.0 - pow(dist / max(radius, 0.001), 4.0), 0.0, 1.0);
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

vec3 detailFloor(vec3 baseColor) {
    vec2 p = vWorldPos.xz;
    float brickBands = 0.5 + 0.5 * sin((p.y + 0.35) * 5.6);
    float brickNoise = fbm(vec3(p.x * 0.85, 3.2, p.y * 1.65));
    float jointNoise = fbm(vec3(p.x * 3.0, 8.4, p.y * 3.0));

    vec3 darkBrick = baseColor * vec3(0.82, 0.82, 0.82);
    vec3 lightBrick = baseColor * vec3(1.03, 1.03, 1.03);
    vec3 detail = mix(darkBrick, lightBrick, brickNoise * 0.55 + brickBands * 0.20);

    float mortarMask = smoothstep(0.74, 0.92, jointNoise) * 0.18;
    detail = mix(detail, baseColor * vec3(0.56, 0.50, 0.48), mortarMask);
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
    if (uMaterialKind == MATERIAL_FLOOR) {
        return detailFloor(baseColor);
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

float materialRoughness() {
    if (uMaterialKind == MATERIAL_WOOD) {
        return 0.74;
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return 0.34;
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return 0.58;
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return 0.94;
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return 0.48;
    }
    if (uMaterialKind == MATERIAL_FLOOR) {
        return 0.86;
    }
    return 0.82;
}

float materialMetalness() {
    if (uMaterialKind == MATERIAL_METAL) {
        return 0.86;
    }
    return 0.0;
}

float materialSpecularLevel() {
    if (uMaterialKind == MATERIAL_WOOD) {
        return 0.24;
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return 1.0;
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return 0.46;
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return 0.10;
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return 0.55;
    }
    if (uMaterialKind == MATERIAL_FLOOR) {
        return 0.10;
    }
    return 0.20;
}

float materialLightTintResponse() {
    if (uMaterialKind == MATERIAL_WOOD) {
        return 0.18;
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return 0.30;
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return 0.24;
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return 0.12;
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return 0.24;
    }
    if (uMaterialKind == MATERIAL_FLOOR) {
        return 0.03;
    }
    return 0.08;
}

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / max(PI * denom * denom, 0.0001);
}

float geometrySchlickGGX(float NdotValue, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotValue / (NdotValue * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float spotConeFactor(RenderLight light, vec3 L) {
    float theta = dot(-L, normalize(light.direction));
    return smoothstep(light.outerConeCos, light.innerConeCos, theta);
}

vec2 shadowTexelSize(int shadowIndex) {
    if (shadowIndex == 0) {
        return 1.0 / vec2(textureSize(uShadowMaps[0], 0));
    }
    return 1.0 / vec2(textureSize(uShadowMaps[1], 0));
}

float shadowDepthAt(int shadowIndex, vec2 uv) {
    if (shadowIndex == 0) {
        return texture(uShadowMaps[0], uv).r;
    }
    return texture(uShadowMaps[1], uv).r;
}

vec4 shadowClipPosition(int shadowIndex) {
    if (shadowIndex == 0) {
        return uShadowMatrices[0] * vec4(vWorldPos, 1.0);
    }
    return uShadowMatrices[1] * vec4(vWorldPos, 1.0);
}

float sampleShadow(int shadowIndex, vec3 N, vec3 L) {
    if (uEnableShadows == 0 || shadowIndex < 0 || shadowIndex >= uShadowCount) {
        return 1.0;
    }

    vec4 clip = shadowClipPosition(shadowIndex);
    if (clip.w <= 0.0) {
        return 1.0;
    }

    vec3 proj = clip.xyz / clip.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) {
        return 1.0;
    }
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) {
        return 1.0;
    }

    float slopeBias = uShadowNormalBias * (1.0 - max(dot(N, L), 0.0));
    float bias = max(uShadowBias, slopeBias);
    vec2 texel = shadowTexelSize(shadowIndex);

    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 offset = vec2(float(x), float(y)) * texel;
            float storedDepth = shadowDepthAt(shadowIndex, proj.xy + offset);
            visibility += (proj.z - bias) <= storedDepth ? 1.0 : 0.0;
        }
    }

    return visibility / 9.0;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 baseColor = clamp(uBaseColor, 0.0, 1.0);
    vec3 albedo = applyMaterialDetail(baseColor, N);

    float roughness = clamp(materialRoughness(), 0.08, 0.98);
    float metalness = clamp(materialMetalness(), 0.0, 1.0);
    float specularLevel = materialSpecularLevel();
    float tintResponse = materialLightTintResponse();

    vec3 ambient = mix(uHemisphereGroundColor, uHemisphereSkyColor, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 totalLight = albedo * ambient * uHemisphereStrength;

    float NdotV = max(dot(N, V), 0.0);
    vec3 dielectricF0 = vec3(0.02 + specularLevel * 0.10);
    vec3 F0 = mix(dielectricF0, albedo, metalness);

    for (int i = 0; i < uNumLights; ++i) {
        RenderLight light = uLights[i];
        vec3 L = vec3(0.0);
        float falloff = 1.0;

        if (light.type == LIGHT_DIRECTIONAL) {
            if (uEnableDirectionalLights == 0) {
                continue;
            }
            L = normalize(-light.direction);
            falloff = uDirectionalLightIntensityScale;
        } else {
            vec3 toLight = light.position - vWorldPos;
            float dist = length(toLight);
            if (dist <= 0.0001 || dist >= light.radius) {
                continue;
            }
            L = toLight / dist;
            falloff = attenuation(dist, light.radius);
            if (light.type == LIGHT_SPOT) {
                float cone = spotConeFactor(light, L);
                if (cone <= 0.0001) {
                    continue;
                }
                falloff *= cone;
            }
        }

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL <= 0.0) {
            continue;
        }

        vec3 H = normalize(V + L);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        float D = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

        vec3 radiance = light.color * light.intensity * falloff;
        float neutralEnergy = luminance(radiance);
        vec3 diffuseRadiance = mix(vec3(neutralEnergy), radiance, tintResponse);
        vec3 specularRadiance = mix(vec3(neutralEnergy), radiance, 0.22 + metalness * 0.68);
        float visibility = (light.castsShadows != 0 && light.shadowIndex >= 0)
            ? sampleShadow(light.shadowIndex, N, L)
            : 1.0;

        totalLight += visibility * ((kD * albedo * diffuseRadiance) + (specular * specularRadiance)) * NdotL;
    }

    fragColor = vec4(max(totalLight, vec3(0.0)), 1.0);

    float materialMarker = (float(uMaterialKind) + 0.5) / 8.0;
    fragNormal = vec4(N * 0.5 + 0.5, materialMarker);
}
