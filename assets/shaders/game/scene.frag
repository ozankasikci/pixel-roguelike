#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vLocalPos;
in vec2 vTexCoord;
in vec3 vObjectPos;
in vec3 vWorldTangent;

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
uniform vec3 uCameraPos;
uniform vec3 uBaseColor;
uniform int uMaterialKind;
uniform float uTimeSeconds;
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAoMap;
uniform int uUseMaterialMaps;
uniform int uMaterialUvMode;
uniform vec2 uMaterialUvScale;
uniform float uMaterialNormalStrength;
uniform float uMaterialRoughnessScale;
uniform float uMaterialRoughnessBias;
uniform float uMaterialMetalness;
uniform float uMaterialAoStrength;
uniform float uMaterialLightTintResponse;

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
const int MATERIAL_BRICK = 7;

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

vec2 dominantProjection(vec3 p, vec3 N) {
    vec3 absNormal = abs(N);
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        return p.xz;
    }
    if (absNormal.x >= absNormal.z) {
        return p.zy;
    }
    return p.xy;
}

void dominantBasis(vec3 N, out vec3 tangent, out vec3 bitangent) {
    vec3 absNormal = abs(N);
    if (absNormal.y >= absNormal.x && absNormal.y >= absNormal.z) {
        tangent = vec3(1.0, 0.0, 0.0);
        bitangent = vec3(0.0, 0.0, sign(N.y == 0.0 ? 1.0 : N.y));
        return;
    }
    if (absNormal.x >= absNormal.z) {
        tangent = vec3(0.0, 0.0, sign(N.x == 0.0 ? 1.0 : N.x));
        bitangent = vec3(0.0, 1.0, 0.0);
        return;
    }
    tangent = vec3(sign(N.z == 0.0 ? 1.0 : N.z), 0.0, 0.0);
    bitangent = vec3(0.0, 1.0, 0.0);
}

float seamMask(vec2 uv, float width) {
    vec2 edgeDistance = min(fract(uv), 1.0 - fract(uv));
    float edge = min(edgeDistance.x, edgeDistance.y);
    return 1.0 - smoothstep(width, width + 0.03, edge);
}

float faceMask(vec2 uv, float start, float end) {
    vec2 centered = abs(fract(uv) - 0.5) * 2.0;
    float edge = max(centered.x, centered.y);
    return 1.0 - smoothstep(start, end, edge);
}

float saturationOf(vec3 color) {
    float maxChannel = max(max(color.r, color.g), color.b);
    float minChannel = min(min(color.r, color.g), color.b);
    return maxChannel - minChannel;
}

float flameMask(vec3 baseColor) {
    return (uMaterialKind == MATERIAL_WAX && saturationOf(baseColor) > 0.28 && baseColor.r > 0.88) ? 1.0 : 0.0;
}

float flameFlicker(vec3 seed) {
    float phase = uTimeSeconds * 8.7 + dot(seed.xz, vec2(1.9, 2.7));
    float pulseA = sin(phase) * 0.5 + 0.5;
    float pulseB = sin(phase * 1.9 + 1.4) * 0.5 + 0.5;
    return 0.88 + (pulseA * 0.62 + pulseB * 0.38) * 0.28;
}

vec4 brickMacroMasks(vec3 N);
vec3 sampleBrickNormalTangent(vec2 uv, vec4 macro);

vec2 materialUv(vec3 N) {
    if (uMaterialUvMode == 2) {
        vec3 projectionSource = (uMaterialKind == MATERIAL_BRICK) ? vWorldPos : vObjectPos;
        return dominantProjection(projectionSource, N) * uMaterialUvScale;
    }
    return vTexCoord * uMaterialUvScale;
}

vec3 applyMaterialMapNormal(vec3 geometricNormal, vec2 uv) {
    vec3 mapped = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    if (uMaterialKind == MATERIAL_BRICK) {
        mapped = sampleBrickNormalTangent(uv, brickMacroMasks(geometricNormal));
    }
    mapped = normalize(vec3(mapped.xy * max(uMaterialNormalStrength, 0.0), mapped.z));

    vec3 tangent;
    vec3 bitangent;
    if (uMaterialUvMode == 2) {
        dominantBasis(geometricNormal, tangent, bitangent);
    } else {
        tangent = normalize(vWorldTangent - geometricNormal * dot(geometricNormal, vWorldTangent));
        if (dot(tangent, tangent) <= 0.0001) {
            dominantBasis(geometricNormal, tangent, bitangent);
        } else {
            bitangent = normalize(cross(geometricNormal, tangent));
        }
    }

    return normalize(tangent * mapped.x + bitangent * mapped.y + geometricNormal * mapped.z);
}

vec4 brickMacroMasks(vec3 N) {
    vec2 macroProjA = dominantProjection(vWorldPos * 0.085, N);
    vec2 macroProjB = dominantProjection((vWorldPos + vec3(31.7, 17.1, 11.3)) * 0.147, N);
    vec2 patchProj = dominantProjection(vWorldPos * 0.062 + vec3(5.3, 1.7, 8.1), N);

    float macroA = fbm(vec3(macroProjA, 2.3));
    float macroB = fbm(vec3(macroProjB, 6.1));
    float patchMask = smoothstep(0.56, 0.84, fbm(vec3(patchProj, 9.4)));
    float dampStreak = smoothstep(0.58, 0.86, fbm(vec3(vWorldPos.x * 0.11 + 4.1, vWorldPos.y * 0.74, vWorldPos.z * 0.05 + 2.6)));
    float dampBase = smoothstep(1.35, -0.45, vWorldPos.y) * smoothstep(0.46, 0.76, macroA);
    return vec4(macroA, macroB, patchMask, clamp(dampBase + dampStreak * 0.55, 0.0, 1.0));
}

vec3 sampleBrickAlbedo(vec2 uv, vec4 macro) {
    return texture(uAlbedoMap, uv).rgb;
}

float sampleBrickRoughness(vec2 uv, vec4 macro) {
    return texture(uRoughnessMap, uv).r;
}

float sampleBrickAo(vec2 uv, vec4 macro) {
    return texture(uAoMap, uv).r;
}

vec3 sampleBrickNormalTangent(vec2 uv, vec4 macro) {
    return texture(uNormalMap, uv).xyz * 2.0 - 1.0;
}

vec3 detailStone(vec3 baseColor, vec3 N) {
    vec2 proj = dominantProjection(vWorldPos * 0.95, N);
    float courseHeight = 0.46;
    float courseIndex = floor(proj.y / courseHeight);
    float courseJitter = mix(0.72, 1.14, valueNoise(vec3(courseIndex * 0.37, 5.2, 0.0)));
    vec2 blockUv = vec2(proj.x / courseJitter, proj.y / courseHeight);
    blockUv.x += mod(courseIndex, 2.0) * 0.5;
    float blockSeam = seamMask(blockUv, 0.048);
    float blockFace = faceMask(blockUv, 0.28, 0.76);
    float blockCavity = smoothstep(0.60, 0.92, fbm(vec3(blockUv * vec2(1.9, 1.4), 6.1)));
    float macro = fbm(vWorldPos * vec3(0.28, 0.20, 0.28));
    float micro = fbm(vWorldPos * 1.9 + N * 0.8);
    float cracks = fbm(vWorldPos * 3.2 + vec3(11.2, 3.4, 7.1));
    float heightWear = clamp((vWorldPos.y + 0.5) * 0.18, 0.0, 1.0);
    float dampMask = smoothstep(1.2, -0.3, vWorldPos.y) * smoothstep(0.45, 0.82, macro);
    float lichenMask = smoothstep(0.52, 0.78, macro) * smoothstep(0.05, 0.85, N.y * 0.5 + 0.5);

    vec3 shadowStone = baseColor * vec3(0.72, 0.78, 0.76);
    vec3 wornStone = baseColor * vec3(1.06, 1.08, 1.04);
    vec3 lichenStone = mix(baseColor * vec3(0.88, 1.00, 0.82), vec3(0.44, 0.53, 0.36), 0.48);
    vec3 detail = mix(shadowStone, wornStone, macro * 0.58 + heightWear * 0.14);
    detail *= 0.90 + micro * 0.15;
    detail = mix(detail, detail * vec3(0.78, 0.82, 0.80), blockSeam * (0.28 + blockCavity * 0.12));
    detail = mix(detail, wornStone, blockFace * 0.06);
    detail = mix(detail, baseColor * vec3(0.50, 0.52, 0.50), dampMask * 0.14);
    detail = mix(detail, shadowStone * vec3(0.62, 0.62, 0.62), smoothstep(0.70, 0.88, cracks) * 0.18);
    detail = mix(detail, lichenStone, lichenMask * 0.12 + dampMask * 0.08);
    return detail;
}

vec3 detailWood(vec3 baseColor) {
    float plankUv = vLocalPos.x * 1.55 + 0.5;
    float plankSeam = 1.0 - smoothstep(0.05, 0.11, min(fract(plankUv), 1.0 - fract(plankUv)));
    float plankBands = 0.5 + 0.5 * sin((vLocalPos.x + 0.1) * 7.0);
    float grain = fbm(vec3(vLocalPos.y * 10.0, vLocalPos.z * 5.0, vLocalPos.x * 3.0) + vWorldPos * 0.35);
    float knots = fbm(vec3(vLocalPos.z * 8.0, vLocalPos.y * 3.0, vLocalPos.x * 8.0));
    float scratches = fbm(vec3(vLocalPos.y * 18.0, vLocalPos.x * 4.0, vLocalPos.z * 10.0));

    vec3 darkWood = baseColor * vec3(0.62, 0.62, 0.62);
    vec3 lightWood = baseColor * vec3(1.02, 1.02, 1.02);
    vec3 detail = mix(darkWood, lightWood, grain);
    detail *= 0.90 + 0.14 * plankBands;
    detail = mix(detail, darkWood * vec3(0.82), plankSeam * 0.32);
    detail = mix(detail, lightWood * vec3(1.02), smoothstep(0.66, 0.88, scratches) * 0.05);
    detail = mix(detail, detail * vec3(0.62, 0.62, 0.62), smoothstep(0.60, 0.85, knots) * 0.24);
    return detail;
}

vec3 detailFloor(vec3 baseColor) {
    vec2 p = vWorldPos.xz;
    vec2 slabUv = vec2(p.x / 1.12, p.y / 0.84);
    slabUv.x += mod(floor(slabUv.y), 2.0) * 0.35;
    float slabSeam = seamMask(slabUv, 0.060);
    float slabFace = faceMask(slabUv, 0.16, 0.76);
    float slabNoise = fbm(vec3(p.x * 0.76, 3.2, p.y * 0.76));
    float slabChips = fbm(vec3(slabUv * 2.4, 8.4));
    float damp = smoothstep(0.52, 0.82, fbm(vec3(p.x * 0.30, 8.1, p.y * 0.30)));

    vec3 darkSlab = baseColor * vec3(0.78, 0.78, 0.78);
    vec3 lightSlab = baseColor * vec3(1.04, 1.04, 1.04);
    vec3 detail = mix(darkSlab, lightSlab, slabNoise * 0.58 + slabFace * 0.14);
    detail = mix(detail, baseColor * vec3(0.52, 0.50, 0.48), slabSeam * (0.44 + slabChips * 0.10));
    detail = mix(detail, detail * vec3(0.84, 0.86, 0.88), damp * 0.10);
    return detail;
}

vec3 detailBrick(vec3 baseColor, vec3 N) {
    vec2 proj = dominantProjection(vWorldPos * 1.05, N);
    float courseHeight = 0.34;
    float courseIndex = floor(proj.y / courseHeight);
    float brickLength = mix(0.72, 1.08, valueNoise(vec3(courseIndex * 0.29, 3.1, 0.0)));
    vec2 brickUv = vec2(proj.x / brickLength, proj.y / courseHeight);
    brickUv.x += mod(courseIndex, 2.0) * 0.5;

    float mortar = seamMask(brickUv, 0.050);
    float face = faceMask(brickUv, 0.18, 0.80);
    float chipMask = smoothstep(0.64, 0.90, fbm(vec3(brickUv * vec2(2.4, 2.2), 4.8)));
    float fireVariation = fbm(vec3(proj * vec2(0.8, 1.0), 2.4));
    float pores = fbm(vec3(proj * vec2(4.2, 4.8), 6.7));
    float soot = smoothstep(0.54, 0.84, fbm(vWorldPos * 0.42 + vec3(8.4, 1.2, 4.7)));
    float damp = smoothstep(1.0, -0.5, vWorldPos.y) * smoothstep(0.52, 0.84, fbm(vWorldPos * 0.65 + vec3(7.3, 2.0, 9.4)));

    vec3 coolBrick = baseColor * vec3(0.84, 0.74, 0.66);
    vec3 warmBrick = baseColor * vec3(1.14, 0.96, 0.78);
    vec3 sootBrick = baseColor * vec3(0.72, 0.62, 0.58);
    vec3 mortarColor = mix(baseColor * vec3(0.70, 0.62, 0.56), vec3(0.66, 0.62, 0.58), 0.55);

    if (uUseMaterialMaps != 0) {
        vec2 uv = materialUv(N);
        vec4 macro = brickMacroMasks(N);
        vec3 mapped = clamp(baseColor * sampleBrickAlbedo(uv, macro), 0.0, 1.0);
        vec3 detail = mapped * (0.98 + pores * 0.05);
        detail *= 0.97 + macro.y * 0.06;
        detail = mix(detail, detail * vec3(1.04, 1.03, 1.01), macro.z * 0.04 + macro.x * 0.02);
        detail = mix(detail, detail * vec3(0.86, 0.82, 0.78), mortar * 0.12);
        detail = mix(detail, mapped * vec3(0.82, 0.77, 0.73), soot * 0.05 + damp * 0.08 + macro.w * 0.08);
        return detail;
    }

    vec3 detail = mix(coolBrick, warmBrick, fireVariation * 0.66 + face * 0.12);
    detail *= 0.90 + pores * 0.12;
    detail = mix(detail, mortarColor, mortar * (0.60 + chipMask * 0.10));
    detail = mix(detail, sootBrick, soot * 0.10 + damp * 0.12);
    return detail;
}

float brickHeight(vec2 proj) {
    float courseHeight = 0.34;
    float courseIndex = floor(proj.y / courseHeight);
    float brickLength = mix(0.72, 1.08, valueNoise(vec3(courseIndex * 0.29, 3.1, 0.0)));
    vec2 brickUv = vec2(proj.x / brickLength, proj.y / courseHeight);
    brickUv.x += mod(courseIndex, 2.0) * 0.5;

    float mortar = seamMask(brickUv, 0.050);
    float face = faceMask(brickUv, 0.18, 0.80);
    float chips = smoothstep(0.64, 0.90, fbm(vec3(brickUv * vec2(2.4, 2.2), 4.8)));
    float pores = fbm(vec3(proj * vec2(4.2, 4.8), 6.7));
    return face * 0.018 - mortar * 0.040 - chips * 0.008 + (pores - 0.5) * 0.006;
}

vec3 detailBrickNormal(vec3 N) {
    vec2 proj = dominantProjection(vWorldPos * 1.05, N);
    vec3 tangent;
    vec3 bitangent;
    dominantBasis(N, tangent, bitangent);

    float eps = 0.035;
    float h = brickHeight(proj);
    float hx = brickHeight(proj + vec2(eps, 0.0));
    float hy = brickHeight(proj + vec2(0.0, eps));
    float dHx = (hx - h) / eps;
    float dHy = (hy - h) / eps;
    float strength = 0.34;
    return normalize(N - tangent * dHx * strength - bitangent * dHy * strength);
}

vec3 detailMetal(vec3 baseColor, vec3 N) {
    float brushed = fbm(vec3(vLocalPos.y * 18.0, vLocalPos.x * 4.0, vLocalPos.z * 4.0));
    float tarnish = fbm(vWorldPos * 2.4 + vLocalPos * 6.0);
    float ridgeSheen = pow(max(dot(N, normalize(vec3(0.3, 0.8, 0.4))), 0.0), 3.0);
    vec3 darkMetal = baseColor * vec3(0.68, 0.70, 0.74);
    vec3 brightMetal = baseColor * vec3(1.04, 1.06, 1.10);
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
    float mask = flameMask(baseColor);
    if (mask > 0.0) {
        float tip = clamp(vLocalPos.y + 0.5, 0.0, 1.0);
        float flicker = flameFlicker(vWorldPos);
        vec3 hotCore = vec3(1.00, 0.88, 0.48);
        vec3 outerFlame = vec3(1.00, 0.58, 0.16);
        detail = mix(detail, outerFlame, mask * (0.18 + tip * 0.18) * flicker);
        detail = mix(detail, hotCore, mask * (0.12 + tip * 0.26) * flicker);
    }
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
    if (uMaterialKind == MATERIAL_BRICK) {
        return detailBrick(baseColor, N);
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
    if (uMaterialKind == MATERIAL_BRICK) {
        return 0.88;
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
    if (uMaterialKind == MATERIAL_BRICK) {
        return 0.08;
    }
    return 0.20;
}

float materialLightTintResponse() {
    if (uMaterialKind == MATERIAL_WOOD) {
        return 0.16;
    }
    if (uMaterialKind == MATERIAL_METAL) {
        return 0.24;
    }
    if (uMaterialKind == MATERIAL_WAX) {
        return 0.18;
    }
    if (uMaterialKind == MATERIAL_MOSS) {
        return 0.14;
    }
    if (uMaterialKind == MATERIAL_VIEWMODEL) {
        return 0.18;
    }
    if (uMaterialKind == MATERIAL_FLOOR) {
        return 0.06;
    }
    if (uMaterialKind == MATERIAL_BRICK) {
        return 0.14;
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
    vec3 geometricNormal = normalize(vNormal);
    vec2 uv = materialUv(geometricNormal);
    vec4 brickMacro = vec4(0.0);
    if (uMaterialKind == MATERIAL_BRICK) {
        brickMacro = brickMacroMasks(geometricNormal);
    }
    vec3 N = geometricNormal;
    if (uUseMaterialMaps != 0) {
        N = applyMaterialMapNormal(geometricNormal, uv);
    } else if (uMaterialKind == MATERIAL_BRICK) {
        N = detailBrickNormal(geometricNormal);
    }
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 baseColor = clamp(uBaseColor, 0.0, 1.0);
    vec3 materialBaseColor = baseColor;
    if (uUseMaterialMaps != 0 && uMaterialKind != MATERIAL_BRICK) {
        materialBaseColor = clamp(baseColor * texture(uAlbedoMap, uv).rgb, 0.0, 1.0);
    }
    vec3 albedo = applyMaterialDetail(materialBaseColor, geometricNormal);

    float roughness = clamp(materialRoughness() * uMaterialRoughnessScale + uMaterialRoughnessBias, 0.08, 0.98);
    float metalness = clamp(uMaterialMetalness, 0.0, 1.0);
    float specularLevel = materialSpecularLevel();
    float tintResponse = clamp(uMaterialLightTintResponse, 0.0, 1.0);
    float materialAo = 1.0;

    if (uUseMaterialMaps != 0) {
        if (uMaterialKind == MATERIAL_BRICK) {
            roughness = clamp(sampleBrickRoughness(uv, brickMacro) * uMaterialRoughnessScale + uMaterialRoughnessBias, 0.08, 0.98);
            materialAo = mix(1.0, sampleBrickAo(uv, brickMacro), clamp(uMaterialAoStrength, 0.0, 1.0));
        } else {
            roughness = clamp(texture(uRoughnessMap, uv).r * uMaterialRoughnessScale + uMaterialRoughnessBias, 0.08, 0.98);
            materialAo = mix(1.0, texture(uAoMap, uv).r, clamp(uMaterialAoStrength, 0.0, 1.0));
        }
        if (uMaterialKind == MATERIAL_BRICK) {
            roughness = clamp(roughness + brickMacro.w * 0.08 + brickMacro.y * 0.02 - brickMacro.z * 0.03, 0.08, 0.98);
            materialAo *= clamp(1.0 - brickMacro.w * 0.12 - brickMacro.z * 0.05, 0.72, 1.0);
        }
    }

    vec3 ambient = mix(uHemisphereGroundColor, uHemisphereSkyColor, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 totalLight = albedo * ambient * uHemisphereStrength * materialAo;

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
        float chromaBoost = smoothstep(0.18, 0.72, saturationOf(light.color));
        float diffuseTintResponse = tintResponse;
        if (uMaterialKind == MATERIAL_STONE) {
            diffuseTintResponse += chromaBoost * 0.10;
        } else if (uMaterialKind == MATERIAL_BRICK) {
            diffuseTintResponse += chromaBoost * 0.14;
        } else if (uMaterialKind == MATERIAL_FLOOR) {
            diffuseTintResponse += chromaBoost * 0.06;
        } else if (uMaterialKind == MATERIAL_WOOD) {
            diffuseTintResponse += chromaBoost * 0.06;
        } else if (uMaterialKind == MATERIAL_WAX) {
            diffuseTintResponse += chromaBoost * 0.04;
        }
        diffuseTintResponse = clamp(diffuseTintResponse, 0.0, 0.42);
        float specularTintResponse = clamp(0.22 + metalness * 0.68 + chromaBoost * 0.08, 0.0, 1.0);
        vec3 diffuseRadiance = mix(vec3(neutralEnergy), radiance, diffuseTintResponse);
        vec3 specularRadiance = mix(vec3(neutralEnergy), radiance, specularTintResponse);
        float visibility = (light.castsShadows != 0 && light.shadowIndex >= 0)
            ? sampleShadow(light.shadowIndex, N, L)
            : 1.0;

        totalLight += visibility * ((kD * albedo * diffuseRadiance) + (specular * specularRadiance)) * NdotL * materialAo;
    }

    float flame = flameMask(baseColor);
    if (flame > 0.0) {
        float tip = clamp(vLocalPos.y + 0.5, 0.0, 1.0);
        totalLight += albedo * flame * (0.16 + tip * 0.24) * flameFlicker(vWorldPos);
    }

    fragColor = vec4(max(totalLight, vec3(0.0)), 1.0);

    float materialMarker = (float(uMaterialKind) + 0.5) / 8.0;
    fragNormal = vec4(N * 0.5 + 0.5, materialMarker);
}
