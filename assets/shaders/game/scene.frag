#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vLocalPos;
in vec2 vTexCoord;
in vec3 vObjectPos;
in vec3 vWorldTangent;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragNormal;
layout(location = 2) out vec4 fragGeomNormal;

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
    vec3 right;        // local X axis (area/tube)
    vec3 up;           // local Y axis (area)
    float width;       // half-width (area) or half-length (tube)
    float height;      // half-height (area) or tube radius
    int doubleSided;   // 0 or 1
};

uniform RenderLight uLights[32];
uniform int uNumLights;
uniform sampler2D uLtcMat;    // 64x64 LTC inverse transform matrix LUT
uniform sampler2D uLtcAmp;    // 64x64 LTC amplitude/Fresnel LUT
uniform sampler2D uShadowMaps[6];
uniform mat4 uShadowMatrices[6];
uniform int uShadowCount;
uniform int uEnableShadows;
uniform float uShadowBias;
uniform float uShadowNormalBias;
uniform mat4 uViewMatrix;
uniform int uDebugViewMode;
uniform sampler2DArrayShadow uCsmShadowMap;
uniform mat4 uCsmMatrices[3];
uniform float uCsmSplitDistances[3];
uniform int uCsmCascadeCount;
uniform int uCsmEnabled;
uniform samplerCube uEnvironmentSpecularMap;
uniform sampler2D uEnvironmentBrdfLut;
uniform int uEnvironmentReflectionsEnabled;
uniform int uEnvironmentSpecularMipCount;
uniform float uEnvironmentReflectionStrength;
uniform vec3 uEnvironmentReflectionTint;
uniform samplerCube uReflectionProbeMap;
uniform int uReflectionProbeEnabled;
uniform vec3 uReflectionProbeCenter;
uniform vec3 uReflectionProbeExtents;
uniform float uReflectionProbeBlendDistance;
uniform float uReflectionProbeIntensity;
uniform int uReflectionProbeBoxProjection;
uniform int uReflectionProbeMipCount;
uniform vec3 uHemisphereSkyColor;
uniform vec3 uHemisphereGroundColor;
uniform float uHemisphereStrength;
uniform int uEnableDirectionalLights;
uniform vec3 uCameraPos;
uniform vec3 uBaseColor;
uniform float uMaterialSpecularLevel;
uniform int uMaterialAnimated;
uniform int uMaterialSubsurface;
uniform int uMaterialBrickDetail;
uniform int uMaterialWoodDetail;
uniform int uMaterialStoneDetail;
uniform int uMaterialFloorDetail;
uniform int uNormalMapFlipY;
uniform int uAlphaTest;
uniform float uAlphaCutoff;
uniform float uTimeSeconds;
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uRoughnessMap;
uniform sampler2D uAoMap;
uniform int uUseMaterialMaps;
uniform int uUseProceduralDetail;
uniform int uMaterialUvMode;
uniform vec2 uMaterialUvScale;
uniform float uMaterialNormalStrength;
uniform float uMaterialRoughnessScale;
uniform float uMaterialRoughnessBias;
uniform float uMaterialMetalness;
uniform float uMaterialAoStrength;
uniform float uMaterialLightTintResponse;
uniform float uEmissiveStrength;
uniform int uWeatheringEnabled;
uniform float uWeatheringDirtStrength;
uniform vec3 uWeatheringDirtColor;
uniform float uWeatheringEdgeWearStrength;
uniform float uWeatheringDustStrength;
uniform float uWeatheringDampStrength;
uniform float uWeatheringNoiseScale;
uniform int uUnlit;

const int LIGHT_POINT = 0;
const int LIGHT_SPOT = 1;
const int LIGHT_DIRECTIONAL = 2;
const int LIGHT_AREA_RECT = 3;
const int LIGHT_TUBE = 4;

const float PI = 3.14159265359;

float attenuation(float dist, float radius) {
    float x = clamp(1.0 - pow(dist / max(radius, 0.001), 4.0), 0.0, 1.0);
    float window = x * x;
    float invSq = 1.0 / (dist * dist + 1.0);
    return window * invSq;
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

// Anti-tiling: per-tile random offset + flip (Inigo Quilez, Technique 1)
// https://iquilezles.org/articles/texturerepetition/
vec4 hash4(vec2 p) {
    return fract(sin(vec4(
        1.0 + dot(p, vec2(37.0, 17.0)),
        2.0 + dot(p, vec2(11.0, 47.0)),
        3.0 + dot(p, vec2(41.0, 29.0)),
        4.0 + dot(p, vec2(23.0, 31.0))
    )) * 103.0);
}

vec4 textureNoTile(sampler2D samp, vec2 uv) {
    ivec2 iuv = ivec2(floor(uv));
    vec2 fuv = fract(uv);

    vec4 ofa = hash4(vec2(iuv + ivec2(0, 0)));
    vec4 ofb = hash4(vec2(iuv + ivec2(1, 0)));
    vec4 ofc = hash4(vec2(iuv + ivec2(0, 1)));
    vec4 ofd = hash4(vec2(iuv + ivec2(1, 1)));

    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);

    ofa.zw = sign(ofa.zw - 0.5);
    ofb.zw = sign(ofb.zw - 0.5);
    ofc.zw = sign(ofc.zw - 0.5);
    ofd.zw = sign(ofd.zw - 0.5);

    vec2 uva = uv * ofa.zw + ofa.xy, ddxa = ddx * ofa.zw, ddya = ddy * ofa.zw;
    vec2 uvb = uv * ofb.zw + ofb.xy, ddxb = ddx * ofb.zw, ddyb = ddy * ofb.zw;
    vec2 uvc = uv * ofc.zw + ofc.xy, ddxc = ddx * ofc.zw, ddyc = ddy * ofc.zw;
    vec2 uvd = uv * ofd.zw + ofd.xy, ddxd = ddx * ofd.zw, ddyd = ddy * ofd.zw;

    vec2 b = smoothstep(0.25, 0.75, fuv);
    return mix(mix(textureGrad(samp, uva, ddxa, ddya),
                   textureGrad(samp, uvb, ddxb, ddyb), b.x),
               mix(textureGrad(samp, uvc, ddxc, ddyc),
                   textureGrad(samp, uvd, ddxd, ddyd), b.x), b.y);
}

vec3 normalMapNoTile(sampler2D samp, vec2 uv) {
    ivec2 iuv = ivec2(floor(uv));
    vec2 fuv = fract(uv);

    vec4 ofa = hash4(vec2(iuv + ivec2(0, 0)));
    vec4 ofb = hash4(vec2(iuv + ivec2(1, 0)));
    vec4 ofc = hash4(vec2(iuv + ivec2(0, 1)));
    vec4 ofd = hash4(vec2(iuv + ivec2(1, 1)));

    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);

    ofa.zw = sign(ofa.zw - 0.5);
    ofb.zw = sign(ofb.zw - 0.5);
    ofc.zw = sign(ofc.zw - 0.5);
    ofd.zw = sign(ofd.zw - 0.5);

    vec2 uva = uv * ofa.zw + ofa.xy, ddxa = ddx * ofa.zw, ddya = ddy * ofa.zw;
    vec2 uvb = uv * ofb.zw + ofb.xy, ddxb = ddx * ofb.zw, ddyb = ddy * ofb.zw;
    vec2 uvc = uv * ofc.zw + ofc.xy, ddxc = ddx * ofc.zw, ddyc = ddy * ofc.zw;
    vec2 uvd = uv * ofd.zw + ofd.xy, ddxd = ddx * ofd.zw, ddyd = ddy * ofd.zw;

    // Unpack and correct tangent-space normals for UV flips
    vec3 na = textureGrad(samp, uva, ddxa, ddya).xyz * 2.0 - 1.0; na.xy *= ofa.zw;
    vec3 nb = textureGrad(samp, uvb, ddxb, ddyb).xyz * 2.0 - 1.0; nb.xy *= ofb.zw;
    vec3 nc = textureGrad(samp, uvc, ddxc, ddyc).xyz * 2.0 - 1.0; nc.xy *= ofc.zw;
    vec3 nd = textureGrad(samp, uvd, ddxd, ddyd).xyz * 2.0 - 1.0; nd.xy *= ofd.zw;

    vec2 b = smoothstep(0.25, 0.75, fuv);
    return normalize(mix(mix(na, nb, b.x), mix(nc, nd, b.x), b.y));
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
    return (uMaterialAnimated != 0 && saturationOf(baseColor) > 0.28 && baseColor.r > 0.88) ? 1.0 : 0.0;
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
        return dominantProjection(vWorldPos, N) * uMaterialUvScale;
    }
    return vTexCoord * uMaterialUvScale;
}

vec3 applyMaterialMapNormal(vec3 geometricNormal, vec2 uv) {
    vec3 mapped;
    if (uUseProceduralDetail == 0 && uMaterialBrickDetail == 0 && uMaterialUvMode != 0) {
        mapped = normalMapNoTile(uNormalMap, uv);
    } else if (uMaterialUvMode == 0 && uMaterialBrickDetail == 0) {
        mapped = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    } else if (uMaterialBrickDetail != 0) {
        mapped = sampleBrickNormalTangent(uv, brickMacroMasks(geometricNormal));
    } else {
        mapped = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    }
    if (uNormalMapFlipY != 0) {
        mapped.y = -mapped.y;
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

vec3 applyMacroVariation(vec3 baseColor, vec3 N) {
    float macro = fbm(vWorldPos * vec3(0.28, 0.20, 0.28));
    float micro = fbm(vWorldPos * 1.9 + N * 0.8);
    float heightWear = clamp((vWorldPos.y + 0.5) * 0.18, 0.0, 1.0);
    float dampMask = smoothstep(1.2, -0.3, vWorldPos.y) * smoothstep(0.45, 0.82, macro);

    vec3 detail = baseColor * (0.94 + macro * 0.10 + heightWear * 0.04);
    detail *= 0.96 + micro * 0.08;
    detail = mix(detail, detail * vec3(0.92, 0.94, 0.96), dampMask * 0.12);
    return detail;
}

// ============================================================
// Weathering utilities — geometry-context signals
// ============================================================

// Screen-space curvature approximation via fwidth() on world normals.
// Returns high values at hard mesh edges (convex features).
float weatherCurvature(vec3 worldNormal) {
    return clamp(length(fwidth(worldNormal)) * 20.0, 0.0, 1.0);
}

// Cavity mask: high in crevices/concavities, low on flat surfaces and convex edges.
float weatherCavityMask(vec3 worldNormal) {
    float curvature = weatherCurvature(worldNormal);
    float underside = clamp(-worldNormal.y * 0.3, 0.0, 0.3);
    return clamp(curvature + underside, 0.0, 1.0);
}

// Edge wear mask: high on convex edges (sharp geometry transitions).
float weatherEdgeWearMask(vec3 worldNormal) {
    return weatherCurvature(worldNormal);
}

// Up-facing mask: 1.0 for surfaces pointing straight up, 0.0 for vertical/downward.
float weatherUpFacingMask(vec3 worldNormal) {
    return clamp(worldNormal.y, 0.0, 1.0);
}

// Height gradient: 0.0 at ground level (y=0), 1.0 at y=4.0 and above.
float weatherHeightGradient(float worldY) {
    return clamp(worldY * 0.25, 0.0, 1.0);
}

// Multi-scale world-space noise for macro variation.
float weatherMacroNoise(vec3 worldPos, float scale) {
    float large = fbm(worldPos * scale * 0.3 + vec3(42.1, 7.3, 19.8));
    float medium = fbm(worldPos * scale * 1.2 + vec3(13.7, 28.4, 5.2));
    return large * 0.65 + medium * 0.35;
}

// ============================================================
// Per-MaterialKind weathering functions
// ============================================================

void weatherStone(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Cavity grime — dark dirt accumulates in crevices
    float dirtMask = cavity * (0.6 + macro * 0.4);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.5);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.15, 0.08, 0.98);

    // Edge wear — lighter, smoother stone on convex edges
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.12, 1.10, 1.08), wearMask * uWeatheringEdgeWearStrength * 0.4);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.1, 0.08, 0.98);

    // Dust on up-facing surfaces
    float dustMask = upFacing * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.04, 1.03, 1.00), dustMask * uWeatheringDustStrength * 0.3);
    roughness = clamp(roughness + dustMask * uWeatheringDustStrength * 0.08, 0.08, 0.98);

    // Damp staining on lower surfaces
    float dampMask = (1.0 - height) * smoothstep(0.4, 0.8, macro);
    albedo = mix(albedo, albedo * vec3(0.88, 0.90, 0.92), dampMask * uWeatheringDampStrength * 0.35);

    // Macro color variation to break uniformity
    albedo *= 0.95 + macro * 0.10;
}

void weatherBrick(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Mortar darkening in joints, soot accumulation
    float dirtMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.45);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.12, 0.08, 0.98);

    // Exposed lighter clay on brick corners
    float wearMask = edge * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.14, 1.08, 1.00), wearMask * uWeatheringEdgeWearStrength * 0.35);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.06, 0.08, 0.98);

    // Dust on top of protruding bricks
    float dustMask = upFacing * (0.3 + macro * 0.7);
    albedo = mix(albedo, albedo * vec3(1.05, 1.04, 1.02), dustMask * uWeatheringDustStrength * 0.25);

    // Efflorescence near ground, soot higher up
    float efflMask = (1.0 - height) * smoothstep(0.45, 0.80, macro);
    albedo = mix(albedo, albedo * vec3(1.08, 1.06, 1.02), efflMask * uWeatheringDampStrength * 0.2);
    float sootMask = height * smoothstep(0.5, 0.85, macro);
    albedo = mix(albedo, albedo * vec3(0.85, 0.82, 0.80), sootMask * uWeatheringDampStrength * 0.2);

    // Macro variation
    albedo *= 0.96 + macro * 0.08;
}

void weatherWood(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Dark grain filling, dirt in plank cracks
    float dirtMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.4);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.1, 0.08, 0.98);

    // Bleached wood on exposed corners
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.16, 1.14, 1.10), wearMask * uWeatheringEdgeWearStrength * 0.3);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.08, 0.08, 0.98);

    // Water stains on horizontal surfaces
    float stainMask = upFacing * smoothstep(0.5, 0.8, macro);
    albedo = mix(albedo, albedo * vec3(0.92, 0.90, 0.86), stainMask * uWeatheringDustStrength * 0.3);

    // Damp/rot near floor
    float dampMask = (1.0 - height) * smoothstep(0.4, 0.75, macro);
    albedo = mix(albedo, albedo * vec3(0.86, 0.84, 0.80), dampMask * uWeatheringDampStrength * 0.3);
    roughness = clamp(roughness + dampMask * uWeatheringDampStrength * 0.12, 0.08, 0.98);

    // Plank-to-plank variation
    albedo *= 0.94 + macro * 0.12;
}

void weatherFloor(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);
    // Floor-specific: spatial zone noise at a different scale for traffic patterns
    float traffic = weatherMacroNoise(worldPos + vec3(77.3, 0.0, 33.1), uWeatheringNoiseScale * 0.6);

    // Grime in slab seams and chips
    float dirtMask = cavity * (0.6 + macro * 0.4);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.5);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.15, 0.08, 0.98);

    // Lighter wear on slab edges
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.08, 1.06, 1.04), wearMask * uWeatheringEdgeWearStrength * 0.3);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.08, 0.08, 0.98);

    // Traffic patterns: worn areas lighter/smoother, neglected areas dirtier/rougher
    float wornZone = smoothstep(0.45, 0.75, traffic);
    albedo = mix(albedo, albedo * vec3(1.06, 1.05, 1.03), wornZone * uWeatheringDustStrength * 0.25);
    roughness = clamp(roughness - wornZone * uWeatheringDustStrength * 0.1, 0.08, 0.98);
    float grimyZone = smoothstep(0.55, 0.85, 1.0 - traffic);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.2, grimyZone * uWeatheringDampStrength * 0.2);
    roughness = clamp(roughness + grimyZone * uWeatheringDampStrength * 0.1, 0.08, 0.98);

    // Macro variation
    albedo *= 0.96 + macro * 0.08;
}

void weatherMetal(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Rust in crevices and joints
    vec3 rustColor = vec3(0.45, 0.22, 0.08);
    float rustMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, rustColor, rustMask * uWeatheringDirtStrength * 0.4);
    roughness = clamp(roughness + rustMask * uWeatheringDirtStrength * 0.25, 0.08, 0.98);

    // Polished bare metal on contact edges
    float polishMask = edge * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.18, 1.16, 1.14), polishMask * uWeatheringEdgeWearStrength * 0.4);
    roughness = clamp(roughness - polishMask * uWeatheringEdgeWearStrength * 0.2, 0.08, 0.98);

    // Water spots on horizontal surfaces
    float spotMask = upFacing * smoothstep(0.55, 0.80, macro);
    albedo = mix(albedo, albedo * vec3(0.92, 0.90, 0.88), spotMask * uWeatheringDustStrength * 0.25);
    roughness = clamp(roughness + spotMask * uWeatheringDustStrength * 0.08, 0.08, 0.98);

    // Drip stains running downward
    float dripMask = (1.0 - height) * smoothstep(0.5, 0.8, macro);
    albedo = mix(albedo, mix(albedo, rustColor, 0.3), dripMask * uWeatheringDampStrength * 0.25);

    // Patchy oxidation
    albedo *= 0.94 + macro * 0.12;
}

// Dispatcher — routes to the correct weathering function based on material kind
void applyMaterialWeathering(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    if (uWeatheringEnabled == 0) return;

    if (uMaterialStoneDetail != 0)      weatherStone(albedo, roughness, N, worldPos);
    else if (uMaterialBrickDetail != 0)  weatherBrick(albedo, roughness, N, worldPos);
    else if (uMaterialWoodDetail != 0)   weatherWood(albedo, roughness, N, worldPos);
    else if (uMaterialFloorDetail != 0)  weatherFloor(albedo, roughness, N, worldPos);
    else if (uMaterialMetalness > 0.5)   weatherMetal(albedo, roughness, N, worldPos);
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
    if (uMaterialWoodDetail != 0) {
        return detailWood(baseColor);
    }
    if (uMaterialBrickDetail != 0) {
        return detailBrick(baseColor, N);
    }
    if (uMaterialFloorDetail != 0) {
        return detailFloor(baseColor);
    }
    if (uMaterialStoneDetail != 0) {
        return detailStone(baseColor, N);
    }
    // Fallback for materials with no detail flag (metal, wax, moss, viewmodel, etc.)
    if (uMaterialMetalness > 0.5) {
        return detailMetal(baseColor, N);
    }
    if (uMaterialAnimated != 0) {
        return detailWax(baseColor, N);
    }
    if (uMaterialSubsurface != 0) {
        return detailMoss(baseColor);
    }
    // Default: apply stone-like macro variation for any unmatched material
    return applyMacroVariation(baseColor, N);
}


float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// LTC helper: integrates one edge of the spherical polygon
vec3 integrateEdgeVec(vec3 v1, vec3 v2) {
    float x = dot(v1, v2);
    float y = abs(x);
    float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
    float b = 3.4175940 + (4.1616724 + y) * y;
    float v = a / b;
    float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;
    return cross(v1, v2) * theta_sintheta;
}

// LTC: evaluate irradiance of a quad light using the LTC approximation.
// Minv: identity for diffuse, inverse LTC transform for specular.
// points[4]: corners of the quad in world space.
float LTC_Evaluate(vec3 N, vec3 V, vec3 P, mat3 Minv, vec3 points[4], bool twoSided) {
    // Build an orthonormal basis around the surface normal
    vec3 T1 = normalize(V - N * dot(V, N));
    vec3 T2 = cross(N, T1);
    Minv = Minv * transpose(mat3(T1, T2, N));

    // Transform quad corners into LTC space
    vec3 L[4];
    L[0] = Minv * (points[0] - P);
    L[1] = Minv * (points[1] - P);
    L[2] = Minv * (points[2] - P);
    L[3] = Minv * (points[3] - P);

    // Back-face check (approximate) using centroid
    vec3 dir = L[0] + L[1] + L[2] + L[3];
    float len = length(dir);
    if (len < 0.0001) return 0.0;
    float z = dir.z / len;
    if (!twoSided && z < 0.0) return 0.0;

    // Project onto unit sphere
    L[0] = normalize(L[0]);
    L[1] = normalize(L[1]);
    L[2] = normalize(L[2]);
    L[3] = normalize(L[3]);

    // Integrate edges of the spherical polygon
    vec3 vsum = vec3(0.0);
    vsum += integrateEdgeVec(L[0], L[1]);
    vsum += integrateEdgeVec(L[1], L[2]);
    vsum += integrateEdgeVec(L[2], L[3]);
    vsum += integrateEdgeVec(L[3], L[0]);

    float result = max(0.0, length(vsum));
    return twoSided ? result : result * (z > 0.0 ? 1.0 : 0.0);
}

// Rectangular area light contribution using LTC
vec3 areaLightContribution(RenderLight light, vec3 N, vec3 V, vec3 P,
                            vec3 albedo, float roughness, float metalness, vec3 F0) {
    vec3 ex = light.right * light.width;
    vec3 ey = light.up * light.height;
    vec3 points[4];
    points[0] = light.position - ex - ey;
    points[1] = light.position + ex - ey;
    points[2] = light.position + ex + ey;
    points[3] = light.position - ex + ey;

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    vec2 ltcUv = vec2(roughness, sqrt(1.0 - NdotV));
    ltcUv = ltcUv * (63.0 / 64.0) + 0.5 / 64.0;

    vec4 t1 = texture(uLtcMat, ltcUv);
    vec4 t2 = texture(uLtcAmp, ltcUv);

    mat3 Minv = mat3(
        vec3(t1.x,   0, t1.y),
        vec3(    0,  1,    0),
        vec3(t1.z,   0, t1.w)
    );

    bool twoSided = light.doubleSided != 0;
    float diffuse  = LTC_Evaluate(N, V, P, mat3(1.0), points, twoSided);
    float specular = LTC_Evaluate(N, V, P, Minv, points, twoSided);
    specular *= t2.x;  // GGX sphere magnitude

    vec3 diff = albedo * (1.0 - metalness) * diffuse;
    vec3 spec = mix(vec3(specular), albedo * specular, metalness);

    float falloff = attenuation(distance(P, light.position), light.radius);
    // Normalise by light area to prevent over-brightness (Pitfall 4 from research)
    float areaNorm = 1.0 / max(light.width * light.height * 4.0, 0.001);
    return (diff + spec) * light.color * light.intensity * areaNorm * falloff;
}

// Tube light contribution using closest-point-on-segment specular
vec3 tubeLightContribution(RenderLight light, vec3 N, vec3 V, vec3 P,
                            vec3 albedo, float roughness, float metalness, vec3 F0) {
    vec3 L0 = light.position - light.right * light.width - P;
    vec3 L1 = light.position + light.right * light.width - P;

    float distL0 = length(L0);
    float distL1 = length(L1);
    float NdotL0 = dot(N, L0 / distL0);
    float NdotL1 = dot(N, L1 / distL1);
    float NdotL = (2.0 * clamp(NdotL0 + NdotL1, 0.0, 2.0))
                / (distL0 * distL1 + dot(L0, L1) + 2.0);

    // Representative point for specular
    vec3 R = reflect(-V, N);
    float RdotL0    = dot(R, L0);
    vec3  Ldelta    = L1 - L0;
    float RdotDelta = dot(R, Ldelta);
    float L0dotD    = dot(L0, Ldelta);
    float deltaD    = dot(Ldelta, Ldelta);
    float denom     = deltaD * deltaD - RdotDelta * RdotDelta;
    float t = clamp((RdotL0 * deltaD - L0dotD * RdotDelta) / (denom + 0.0001), 0.0, 1.0);
    vec3 closestPoint = L0 + Ldelta * t;

    float tubeRadius = light.height;
    vec3  centerToRay = closestPoint - R * dot(R, closestPoint);
    float ctrLen      = length(centerToRay);
    vec3  closestOnSphere = closestPoint;
    if (ctrLen > 0.001) {
        closestOnSphere += normalize(centerToRay) * min(tubeRadius, ctrLen);
    }

    vec3  L       = normalize(closestOnSphere);
    float alphaPrime = clamp(roughness + tubeRadius / (2.0 * max(length(closestPoint), 0.001)), 0.0, 1.0);

    float NdotLr = max(dot(N, L), 0.0);
    vec3  H      = normalize(V + L);
    float NdotH  = max(dot(N, H), 0.0);
    float NdotVr = max(dot(N, V), 0.001);

    float a2    = alphaPrime * alphaPrime;
    float a4    = a2 * a2;
    float dH    = NdotH * NdotH;
    float denom2 = dH * (a4 - 1.0) + 1.0;
    float D = a4 / max(PI * denom2 * denom2, 0.0001);

    float rg = alphaPrime + 1.0;
    float k  = (rg * rg) / 8.0;
    float G  = (NdotVr / (NdotVr * (1.0 - k) + k)) * (NdotLr / (NdotLr * (1.0 - k) + k));

    vec3  F    = F0 + (1.0 - F0) * pow(clamp(1.0 - dot(H, V), 0.0, 1.0), 5.0);
    vec3  spec = (D * G * F) / (4.0 * NdotVr * NdotLr + 0.0001);
    vec3  kD   = (vec3(1.0) - F) * (1.0 - metalness);
    vec3  diff = kD * albedo / PI;

    float falloff = attenuation(length(closestPoint), light.radius);
    return (diff * max(NdotL, 0.0) + spec * NdotLr)
         * light.color * light.intensity * falloff;
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    vec3 edgeReflectance = max(vec3(1.0 - roughness), F0);
    return F0 + (edgeReflectance - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float spotConeFactor(RenderLight light, vec3 L) {
    float theta = dot(-L, normalize(light.direction));
    return smoothstep(light.outerConeCos, light.innerConeCos, theta);
}

vec2 shadowTexelSize(int shadowIndex) {
    if (shadowIndex == 0) return 1.0 / vec2(textureSize(uShadowMaps[0], 0));
    if (shadowIndex == 1) return 1.0 / vec2(textureSize(uShadowMaps[1], 0));
    if (shadowIndex == 2) return 1.0 / vec2(textureSize(uShadowMaps[2], 0));
    if (shadowIndex == 3) return 1.0 / vec2(textureSize(uShadowMaps[3], 0));
    if (shadowIndex == 4) return 1.0 / vec2(textureSize(uShadowMaps[4], 0));
    return 1.0 / vec2(textureSize(uShadowMaps[5], 0));
}

float shadowDepthAt(int shadowIndex, vec2 uv) {
    if (shadowIndex == 0) return texture(uShadowMaps[0], uv).r;
    if (shadowIndex == 1) return texture(uShadowMaps[1], uv).r;
    if (shadowIndex == 2) return texture(uShadowMaps[2], uv).r;
    if (shadowIndex == 3) return texture(uShadowMaps[3], uv).r;
    if (shadowIndex == 4) return texture(uShadowMaps[4], uv).r;
    return texture(uShadowMaps[5], uv).r;
}

vec4 shadowClipPosition(int shadowIndex) {
    if (shadowIndex == 0) return uShadowMatrices[0] * vec4(vWorldPos, 1.0);
    if (shadowIndex == 1) return uShadowMatrices[1] * vec4(vWorldPos, 1.0);
    if (shadowIndex == 2) return uShadowMatrices[2] * vec4(vWorldPos, 1.0);
    if (shadowIndex == 3) return uShadowMatrices[3] * vec4(vWorldPos, 1.0);
    if (shadowIndex == 4) return uShadowMatrices[4] * vec4(vWorldPos, 1.0);
    return uShadowMatrices[5] * vec4(vWorldPos, 1.0);
}

const vec2 poissonDisk[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
);

float hash12(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

int selectCascade(float viewDepth) {
    for (int i = 0; i < uCsmCascadeCount; ++i) {
        if (viewDepth < uCsmSplitDistances[i]) return i;
    }
    return uCsmCascadeCount - 1;
}

bool computeCsmProjection(vec3 fragWorldPos, vec3 fragViewPos, out int layer, out vec3 projCoords) {
    if (uCsmEnabled == 0 || uCsmCascadeCount <= 0) {
        layer = 0;
        projCoords = vec3(0.0);
        return false;
    }

    layer = selectCascade(abs(fragViewPos.z));
    vec4 fragInLightSpace = uCsmMatrices[layer] * vec4(fragWorldPos, 1.0);
    projCoords = fragInLightSpace.xyz / fragInLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    return true;
}

bool csmUvInBounds(vec3 projCoords) {
    return projCoords.x > 0.0 && projCoords.x < 1.0
        && projCoords.y > 0.0 && projCoords.y < 1.0;
}

bool csmDepthInBounds(vec3 projCoords) {
    return projCoords.z > 0.0 && projCoords.z < 1.0;
}

vec3 cascadeDebugColor(int layer) {
    if (layer == 0) return vec3(1.0, 0.25, 0.25);
    if (layer == 1) return vec3(0.25, 1.0, 0.35);
    if (layer == 2) return vec3(0.25, 0.55, 1.0);
    return vec3(1.0);
}

float sampleCsmShadow(vec3 fragWorldPos, vec3 fragViewPos, vec3 N, vec3 L) {
    if (uCsmEnabled == 0) return 1.0;
    int layer = 0;
    vec3 projCoords = vec3(0.0);
    if (!computeCsmProjection(fragWorldPos, fragViewPos, layer, projCoords)) {
        return 1.0;
    }
    if (!csmUvInBounds(projCoords)) return 1.0;
    if (projCoords.z >= 1.0) return 1.0;

    // Directional shadow bias works in cascade depth space, so it needs to stay much
    // smaller than the spot-light bias values exposed in the editor. Scale the shared
    // controls down and use the geometric receiver normal to reduce contact light leaks.
    float receiverSlope = 1.0 - max(dot(N, L), 0.0);
    float constantBias = 0.0;
    float normalBias = 0.0 * receiverSlope;
    float baseBias = max(constantBias, normalBias);
    float bias = baseBias / max(uCsmSplitDistances[layer], 0.01);

    // 16-tap Poisson PCF matching spot shadow quality
    float angle = hash12(gl_FragCoord.xy) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, s, -s, c);
    float texelSize = 1.0 / float(textureSize(uCsmShadowMap, 0).x);
    float spread = 1.0;

    float centerVisibility = texture(uCsmShadowMap,
        vec4(projCoords.xy, float(layer), projCoords.z - bias));
    float visibility = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = (rot * poissonDisk[i]) * texelSize * spread;
        visibility += texture(uCsmShadowMap,
            vec4(projCoords.xy + offset, float(layer), projCoords.z - bias));
    }
    visibility /= 16.0;
    visibility = min(centerVisibility, visibility);

    // Blend between cascades in a 10% overlap zone to prevent visible seams
    if (layer < uCsmCascadeCount - 1) {
        float splitDist = uCsmSplitDistances[layer];
        float blendZone = splitDist * 0.1;
        float distFromSplit = splitDist - abs(fragViewPos.z);
        if (distFromSplit < blendZone && distFromSplit > 0.0) {
            float blendFactor = 1.0 - distFromSplit / blendZone;
            int nextLayer = layer + 1;
            vec4 nextLightSpace = uCsmMatrices[nextLayer] * vec4(fragWorldPos, 1.0);
            vec3 nextProj = nextLightSpace.xyz / nextLightSpace.w * 0.5 + 0.5;
            float nextBias = baseBias / max(uCsmSplitDistances[nextLayer], 0.01);
            float nextCenterVisibility = texture(uCsmShadowMap,
                vec4(nextProj.xy, float(nextLayer), nextProj.z - nextBias));
            float nextVisibility = 0.0;
            for (int i = 0; i < 16; ++i) {
                vec2 offset = (rot * poissonDisk[i]) * texelSize * spread;
                nextVisibility += texture(uCsmShadowMap,
                    vec4(nextProj.xy + offset, float(nextLayer), nextProj.z - nextBias));
            }
            nextVisibility /= 16.0;
            nextVisibility = min(nextCenterVisibility, nextVisibility);
            visibility = mix(visibility, nextVisibility, blendFactor);
        }
    }

    return visibility;
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
    float spread = 3.0;

    float angle = hash12(gl_FragCoord.xy) * 6.2831853;
    float s = sin(angle), c = cos(angle);
    mat2 rot = mat2(c, s, -s, c);

    float visibility = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = (rot * poissonDisk[i]) * texel * spread;
        float storedDepth = shadowDepthAt(shadowIndex, proj.xy + offset);
        visibility += (proj.z - bias) <= storedDepth ? 1.0 : 0.0;
    }
    return visibility / 16.0;
}

vec3 sampleEnvironmentSpecular(vec3 N, vec3 V, vec3 F0, float roughness) {
    if (uEnvironmentReflectionsEnabled == 0 || uEnvironmentSpecularMipCount <= 0) {
        return vec3(0.0);
    }

    vec3 R = reflect(-V, N);
    vec3 envDir = normalize(vec3(R.x, R.y, -R.z));
    float maxMip = float(max(uEnvironmentSpecularMipCount - 1, 0));
    vec3 prefiltered = textureLod(uEnvironmentSpecularMap, envDir, roughness * maxMip).rgb;
    vec2 brdf = texture(uEnvironmentBrdfLut, vec2(clamp(dot(N, V), 0.0, 1.0), roughness)).rg;
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 reflectionTint = max(uEnvironmentReflectionTint, vec3(0.0));
    float reflectionStrength = max(uEnvironmentReflectionStrength, 0.0);
    vec3 iblSpecular = prefiltered * reflectionTint * (F * brdf.x + brdf.y);
    return iblSpecular * reflectionStrength;
}

float distanceOutsideBox(vec3 point, vec3 center, vec3 extents) {
    vec3 delta = abs(point - center) - extents;
    return length(max(delta, vec3(0.0)));
}

vec3 boxProjectReflection(vec3 reflectionDir, vec3 worldPos, vec3 probeCenter, vec3 probeExtents) {
    vec3 boxMin = probeCenter - probeExtents;
    vec3 boxMax = probeCenter + probeExtents;
    vec3 safeSign = mix(vec3(-1.0), vec3(1.0), step(vec3(0.0), reflectionDir));
    vec3 safeDir = mix(reflectionDir, safeSign * vec3(0.0001), lessThan(abs(reflectionDir), vec3(0.0001)));

    vec3 firstPlane = (boxMin - worldPos) / safeDir;
    vec3 secondPlane = (boxMax - worldPos) / safeDir;
    vec3 furthestPlane = max(firstPlane, secondPlane);
    float travel = min(furthestPlane.x, min(furthestPlane.y, furthestPlane.z));
    vec3 hitPosition = worldPos + safeDir * travel;
    return hitPosition - probeCenter;
}

vec3 sampleLocalProbeSpecular(vec3 N, vec3 V, vec3 F0, float roughness) {
    if (uReflectionProbeEnabled == 0 || uReflectionProbeMipCount <= 0) {
        return vec3(0.0);
    }

    vec3 R = reflect(-V, N);
    vec3 probeDir = R;
    if (uReflectionProbeBoxProjection != 0) {
        probeDir = boxProjectReflection(R, vWorldPos, uReflectionProbeCenter, uReflectionProbeExtents);
    }
    vec3 sampleDir = normalize(vec3(probeDir.x, probeDir.y, -probeDir.z));
    float maxMip = float(max(uReflectionProbeMipCount - 1, 0));
    vec3 prefiltered = textureLod(uReflectionProbeMap, sampleDir, roughness * maxMip).rgb;
    vec2 brdf = texture(uEnvironmentBrdfLut, vec2(clamp(dot(N, V), 0.0, 1.0), roughness)).rg;
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 iblSpecular = prefiltered * (F * brdf.x + brdf.y);
    return iblSpecular * max(uReflectionProbeIntensity, 0.0);
}

void main() {
    vec3 geometricNormal = normalize(vNormal);
    fragGeomNormal = vec4(geometricNormal * 0.5 + 0.5, 1.0);
    vec2 uv = materialUv(geometricNormal);
    vec4 brickMacro = vec4(0.0);
    if (uMaterialBrickDetail != 0) {
        brickMacro = brickMacroMasks(geometricNormal);
    }
    vec3 N = geometricNormal;
    if (uUseMaterialMaps != 0) {
        N = applyMaterialMapNormal(geometricNormal, uv);
    } else if (uMaterialBrickDetail != 0) {
        N = detailBrickNormal(geometricNormal);
    }
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 fragViewPos = (uViewMatrix * vec4(vWorldPos, 1.0)).xyz;
    vec3 baseColor = clamp(uBaseColor, 0.0, 1.0);
    if (uUnlit != 0) {
        float materialMarker = 0.0;
        if (uMaterialBrickDetail != 0) materialMarker = 0.875;
        else if (uMaterialFloorDetail != 0) materialMarker = 0.8125;
        else if (uMaterialWoodDetail != 0) materialMarker = 0.1875;
        else if (uMaterialStoneDetail != 0) materialMarker = 0.0625;
        else if (uMaterialAnimated != 0) materialMarker = 0.4375;
        else if (uMaterialSubsurface != 0) materialMarker = 0.5625;
        else if (uMaterialMetalness > 0.5) materialMarker = 0.3125;
        else materialMarker = 0.6875;
        fragColor = vec4(baseColor, 1.0);
        fragNormal = vec4(N * 0.5 + 0.5, materialMarker);
        // fragGeomNormal already written above
        return;
    }
    vec3 materialBaseColor = baseColor;
    if (uUseMaterialMaps != 0 && uMaterialBrickDetail == 0) {
        vec4 albedoRGBA = (uUseProceduralDetail == 0 && uMaterialUvMode != 0)
            ? textureNoTile(uAlbedoMap, uv)
            : texture(uAlbedoMap, uv);
        if (uAlphaTest != 0 && albedoRGBA.a < uAlphaCutoff) {
            discard;
        }
        materialBaseColor = clamp(baseColor * albedoRGBA.rgb, 0.0, 1.0);
    }
    vec3 albedo = (uUseProceduralDetail != 0 || uUseMaterialMaps == 0)
        ? applyMaterialDetail(materialBaseColor, geometricNormal)
        : applyMacroVariation(materialBaseColor, geometricNormal);

    float roughness = clamp(uMaterialRoughnessScale * uMaterialRoughnessBias, 0.08, 0.98);
    float metalness = clamp(uMaterialMetalness, 0.0, 1.0);
    float specularLevel = uMaterialSpecularLevel;
    float tintResponse = clamp(uMaterialLightTintResponse, 0.0, 1.0);
    float materialAo = 1.0;

    if (uUseMaterialMaps != 0) {
        if (uMaterialBrickDetail != 0) {
            roughness = clamp(sampleBrickRoughness(uv, brickMacro) * uMaterialRoughnessScale + uMaterialRoughnessBias, 0.08, 0.98);
            materialAo = mix(1.0, sampleBrickAo(uv, brickMacro), clamp(uMaterialAoStrength, 0.0, 1.0));
        } else {
            float roughSample = (uUseProceduralDetail == 0 && uMaterialUvMode != 0)
                ? textureNoTile(uRoughnessMap, uv).r
                : texture(uRoughnessMap, uv).r;
            float aoSample = (uUseProceduralDetail == 0 && uMaterialUvMode != 0)
                ? textureNoTile(uAoMap, uv).r
                : texture(uAoMap, uv).r;
            roughness = clamp(roughSample * uMaterialRoughnessScale + uMaterialRoughnessBias, 0.08, 0.98);
            materialAo = mix(1.0, aoSample, clamp(uMaterialAoStrength, 0.0, 1.0));
        }
        if (uMaterialBrickDetail != 0) {
            roughness = clamp(roughness + brickMacro.w * 0.08 + brickMacro.y * 0.02 - brickMacro.z * 0.03, 0.08, 0.98);
            materialAo *= clamp(1.0 - brickMacro.w * 0.12 - brickMacro.z * 0.05, 0.72, 1.0);
        }
    }

    // Per-material weathering — modifies albedo and roughness based on geometry context
    applyMaterialWeathering(albedo, roughness, geometricNormal, vWorldPos);

    vec3 ambient = mix(uHemisphereGroundColor, uHemisphereSkyColor, clamp(N.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 totalLight = albedo * ambient * uHemisphereStrength * materialAo;
    vec3 sunDirectDebug = vec3(0.0);
    float sunShadowVisibilityDebug = 0.0;
    bool sunShadowValidDebug = false;
    bool csmPrimaryUvOutOfBoundsDebug = false;
    bool csmPrimaryDepthOutOfBoundsDebug = false;
    bool csmNextUvOutOfBoundsDebug = false;
    bool csmNextDepthOutOfBoundsDebug = false;
    int csmCascadeDebug = -1;

    float NdotV = max(dot(N, V), 0.0);
    vec3 dielectricF0 = vec3(0.02 + specularLevel * 0.10);
    vec3 F0 = mix(dielectricF0, albedo, metalness);
    vec3 iblSpecular = sampleEnvironmentSpecular(N, V, F0, roughness);
    if (uReflectionProbeEnabled != 0) {
        float blendDistance = max(uReflectionProbeBlendDistance, 0.001);
        float outsideDistance = distanceOutsideBox(vWorldPos, uReflectionProbeCenter, uReflectionProbeExtents);
        float localBlend = 1.0 - smoothstep(0.0, blendDistance, outsideDistance);
        vec3 localProbeSpecular = sampleLocalProbeSpecular(N, V, F0, roughness);
        iblSpecular = mix(iblSpecular, localProbeSpecular, localBlend);
    }
    totalLight += iblSpecular * materialAo;

    for (int i = 0; i < uNumLights; ++i) {
        RenderLight light = uLights[i];

        // Area rect and tube lights use dedicated evaluation functions
        if (light.type == LIGHT_AREA_RECT) {
            totalLight += areaLightContribution(light, N, V, vWorldPos,
                                                albedo, roughness, metalness, F0) * materialAo;
            continue;
        }
        if (light.type == LIGHT_TUBE) {
            totalLight += tubeLightContribution(light, N, V, vWorldPos,
                                                albedo, roughness, metalness, F0) * materialAo;
            continue;
        }

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
        vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.01);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metalness);

        vec3 radiance = light.color * light.intensity * falloff;
        float neutralEnergy = luminance(radiance);
        float chromaBoost = smoothstep(0.18, 0.72, saturationOf(light.color));
        float diffuseTintResponse = tintResponse;
        if (uMaterialStoneDetail != 0) {
            diffuseTintResponse += chromaBoost * 0.10;
        } else if (uMaterialBrickDetail != 0) {
            diffuseTintResponse += chromaBoost * 0.14;
        } else if (uMaterialFloorDetail != 0) {
            diffuseTintResponse += chromaBoost * 0.06;
        } else if (uMaterialWoodDetail != 0) {
            diffuseTintResponse += chromaBoost * 0.06;
        } else if (uMaterialAnimated != 0) {
            diffuseTintResponse += chromaBoost * 0.04;
        }
        diffuseTintResponse = clamp(diffuseTintResponse, 0.0, 0.42);
        float specularTintResponse = clamp(0.22 + metalness * 0.68 + chromaBoost * 0.08, 0.0, 1.0);
        vec3 diffuseRadiance = mix(vec3(neutralEnergy), radiance, diffuseTintResponse);
        vec3 specularRadiance = mix(vec3(neutralEnergy), radiance, specularTintResponse);
        float visibility = 1.0;
        if (light.type == LIGHT_DIRECTIONAL) {
            int csmLayer = 0;
            vec3 csmProj = vec3(0.0);
            if (computeCsmProjection(vWorldPos, fragViewPos, csmLayer, csmProj)) {
                csmCascadeDebug = csmLayer;
                csmPrimaryUvOutOfBoundsDebug = !csmUvInBounds(csmProj);
                csmPrimaryDepthOutOfBoundsDebug = !csmDepthInBounds(csmProj);

                if (csmLayer < uCsmCascadeCount - 1) {
                    float splitDist = uCsmSplitDistances[csmLayer];
                    float blendZone = splitDist * 0.1;
                    float distFromSplit = splitDist - abs(fragViewPos.z);
                    if (distFromSplit < blendZone && distFromSplit > 0.0) {
                        int nextLayer = csmLayer + 1;
                        vec4 nextLightSpace = uCsmMatrices[nextLayer] * vec4(vWorldPos, 1.0);
                        vec3 nextProj = nextLightSpace.xyz / nextLightSpace.w * 0.5 + 0.5;
                        csmNextUvOutOfBoundsDebug = !csmUvInBounds(nextProj);
                        csmNextDepthOutOfBoundsDebug = !csmDepthInBounds(nextProj);
                    }
                }
            }
            visibility = sampleCsmShadow(vWorldPos, fragViewPos, geometricNormal, L);
            sunShadowVisibilityDebug = visibility;
            sunShadowValidDebug = true;
        } else if (light.castsShadows != 0 && light.shadowIndex >= 0) {
            visibility = sampleShadow(light.shadowIndex, geometricNormal, L);
        }

        vec3 lightContribution = visibility * ((kD * albedo * diffuseRadiance) + (specular * specularRadiance)) * NdotL * materialAo;
        totalLight += lightContribution;
        if (light.type == LIGHT_DIRECTIONAL) {
            sunDirectDebug += lightContribution;
        }
    }

    if (uDebugViewMode == 5) {
        fragColor = vec4(max(sunDirectDebug, vec3(0.0)), 1.0);
        float marker = csmCascadeDebug >= 0 ? (float(csmCascadeDebug) + 1.0) / 8.0 : 0.0;
        fragNormal = vec4(N * 0.5 + 0.5, marker);
        return;
    }
    if (uDebugViewMode == 6) {
        float value = sunShadowValidDebug ? sunShadowVisibilityDebug : 0.0;
        fragColor = vec4(vec3(value), 1.0);
        float marker = csmCascadeDebug >= 0 ? (float(csmCascadeDebug) + 1.0) / 8.0 : 0.0;
        fragNormal = vec4(N * 0.5 + 0.5, marker);
        return;
    }
    if (uDebugViewMode == 7) {
        vec3 debugColor = vec3(0.06);
        if (csmPrimaryDepthOutOfBoundsDebug || csmNextDepthOutOfBoundsDebug) {
            debugColor = vec3(0.20, 0.45, 1.0);
        } else if (csmPrimaryUvOutOfBoundsDebug) {
            debugColor = vec3(1.0, 0.18, 0.18);
        } else if (csmNextUvOutOfBoundsDebug) {
            debugColor = vec3(1.0, 0.72, 0.18);
        } else if (sunShadowValidDebug) {
            debugColor = vec3(0.16, 0.82, 0.26);
        }
        fragColor = vec4(debugColor, 1.0);
        float marker = csmCascadeDebug >= 0 ? (float(csmCascadeDebug) + 1.0) / 8.0 : 0.0;
        fragNormal = vec4(N * 0.5 + 0.5, marker);
        return;
    }
    if (uDebugViewMode == 8) {
        vec3 debugColor = csmCascadeDebug >= 0 ? cascadeDebugColor(csmCascadeDebug) : vec3(0.0);
        fragColor = vec4(debugColor, 1.0);
        float marker = csmCascadeDebug >= 0 ? (float(csmCascadeDebug) + 1.0) / 8.0 : 0.0;
        fragNormal = vec4(N * 0.5 + 0.5, marker);
        return;
    }

    // Emissive self-illumination (combined with bloom this produces perceived light bleeding)
    totalLight += albedo * max(uEmissiveStrength, 0.0);

    float flame = flameMask(baseColor);
    if (flame > 0.0) {
        float tip = clamp(vLocalPos.y + 0.5, 0.0, 1.0);
        totalLight += albedo * flame * (0.16 + tip * 0.24) * flameFlicker(vWorldPos);
    }

    fragColor = vec4(max(totalLight, vec3(0.0)), 1.0);

    float materialMarker = 0.0;
    if (uMaterialBrickDetail != 0) materialMarker = 0.875;
    else if (uMaterialFloorDetail != 0) materialMarker = 0.8125;
    else if (uMaterialWoodDetail != 0) materialMarker = 0.1875;
    else if (uMaterialStoneDetail != 0) materialMarker = 0.0625;
    else if (uMaterialAnimated != 0) materialMarker = 0.4375;
    else if (uMaterialSubsurface != 0) materialMarker = 0.5625;
    else if (uMaterialMetalness > 0.5) materialMarker = 0.3125;
    else materialMarker = 0.6875;
    fragNormal = vec4(N * 0.5 + 0.5, materialMarker);
}
