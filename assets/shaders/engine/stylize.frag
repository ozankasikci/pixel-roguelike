#version 410 core

uniform sampler2D compositeColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;

uniform int uEnableDither;
uniform int uEnableEdges;
uniform int uDebugViewMode;
uniform int uPaletteVariant;
uniform float uThresholdBias;
uniform float uPatternScale;
uniform float uEdgeThreshold;
uniform float uDepthViewScale;
uniform float uFogStart;
uniform float uNearPlane;
uniform float uFarPlane;

in vec2 vTexCoord;
out vec4 fragColor;

const vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);
const vec3 neutralInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.03, 0.03, 0.04),
    vec3(0.07, 0.07, 0.08),
    vec3(0.12, 0.12, 0.13),
    vec3(0.18, 0.17, 0.16),
    vec3(0.25, 0.23, 0.21),
    vec3(0.33, 0.30, 0.27),
    vec3(0.42, 0.38, 0.33),
    vec3(0.52, 0.47, 0.41),
    vec3(0.63, 0.58, 0.51),
    vec3(0.74, 0.69, 0.60),
    vec3(0.84, 0.78, 0.68),
    vec3(0.92, 0.87, 0.77),
    vec3(0.62, 0.64, 0.69),
    vec3(0.39, 0.36, 0.31)
);
const vec3 dungeonInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.02, 0.02, 0.01),
    vec3(0.06, 0.04, 0.02),
    vec3(0.11, 0.08, 0.04),
    vec3(0.17, 0.12, 0.06),
    vec3(0.24, 0.17, 0.09),
    vec3(0.33, 0.23, 0.12),
    vec3(0.43, 0.30, 0.16),
    vec3(0.55, 0.39, 0.21),
    vec3(0.67, 0.48, 0.27),
    vec3(0.79, 0.58, 0.34),
    vec3(0.92, 0.67, 0.28),
    vec3(1.00, 0.82, 0.38),
    vec3(0.84, 0.68, 0.43),
    vec3(0.50, 0.39, 0.24)
);
const vec3 meadowInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.03, 0.05, 0.05),
    vec3(0.06, 0.09, 0.08),
    vec3(0.10, 0.14, 0.11),
    vec3(0.15, 0.19, 0.13),
    vec3(0.21, 0.26, 0.15),
    vec3(0.29, 0.35, 0.19),
    vec3(0.38, 0.46, 0.24),
    vec3(0.50, 0.60, 0.31),
    vec3(0.65, 0.73, 0.39),
    vec3(0.82, 0.83, 0.50),
    vec3(0.93, 0.80, 0.46),
    vec3(0.66, 0.76, 0.92),
    vec3(0.89, 0.67, 0.34),
    vec3(0.37, 0.29, 0.19)
);
const vec3 duskInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.03, 0.03, 0.05),
    vec3(0.06, 0.06, 0.10),
    vec3(0.10, 0.10, 0.16),
    vec3(0.15, 0.15, 0.22),
    vec3(0.21, 0.22, 0.30),
    vec3(0.30, 0.31, 0.41),
    vec3(0.40, 0.41, 0.54),
    vec3(0.52, 0.52, 0.66),
    vec3(0.64, 0.62, 0.75),
    vec3(0.77, 0.72, 0.78),
    vec3(0.88, 0.82, 0.68),
    vec3(0.59, 0.67, 0.81),
    vec3(0.36, 0.28, 0.34),
    vec3(0.22, 0.18, 0.18)
);
const vec3 arcaneInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.02, 0.04, 0.04),
    vec3(0.05, 0.08, 0.08),
    vec3(0.08, 0.13, 0.12),
    vec3(0.12, 0.20, 0.18),
    vec3(0.18, 0.29, 0.23),
    vec3(0.26, 0.40, 0.27),
    vec3(0.38, 0.55, 0.31),
    vec3(0.55, 0.71, 0.35),
    vec3(0.74, 0.78, 0.39),
    vec3(0.90, 0.78, 0.34),
    vec3(0.29, 0.62, 0.78),
    vec3(0.53, 0.83, 0.90),
    vec3(0.77, 0.32, 0.20),
    vec3(0.40, 0.28, 0.12)
);
const vec3 cathedralArcadeInkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.03, 0.03, 0.04),
    vec3(0.07, 0.07, 0.08),
    vec3(0.11, 0.11, 0.12),
    vec3(0.16, 0.16, 0.16),
    vec3(0.22, 0.22, 0.21),
    vec3(0.30, 0.30, 0.28),
    vec3(0.40, 0.40, 0.38),
    vec3(0.54, 0.54, 0.50),
    vec3(0.70, 0.69, 0.64),
    vec3(0.84, 0.82, 0.76),
    vec3(0.58, 0.61, 0.63),
    vec3(0.44, 0.47, 0.39),
    vec3(0.46, 0.34, 0.26),
    vec3(0.35, 0.23, 0.22)
);
const vec3 neutralFloorPalette[6] = vec3[6](
    vec3(0.08, 0.08, 0.08),
    vec3(0.15, 0.14, 0.13),
    vec3(0.24, 0.22, 0.19),
    vec3(0.35, 0.31, 0.27),
    vec3(0.49, 0.43, 0.37),
    vec3(0.66, 0.59, 0.51)
);
const vec3 dungeonFloorPalette[6] = vec3[6](
    vec3(0.07, 0.05, 0.03),
    vec3(0.13, 0.09, 0.05),
    vec3(0.21, 0.15, 0.08),
    vec3(0.31, 0.22, 0.11),
    vec3(0.44, 0.31, 0.16),
    vec3(0.60, 0.43, 0.23)
);
const vec3 meadowFloorPalette[6] = vec3[6](
    vec3(0.10, 0.08, 0.04),
    vec3(0.19, 0.14, 0.06),
    vec3(0.31, 0.23, 0.10),
    vec3(0.46, 0.35, 0.16),
    vec3(0.64, 0.50, 0.24),
    vec3(0.82, 0.69, 0.39)
);
const vec3 duskFloorPalette[6] = vec3[6](
    vec3(0.05, 0.05, 0.08),
    vec3(0.11, 0.10, 0.15),
    vec3(0.20, 0.18, 0.27),
    vec3(0.31, 0.28, 0.39),
    vec3(0.45, 0.41, 0.55),
    vec3(0.63, 0.58, 0.70)
);
const vec3 arcaneFloorPalette[6] = vec3[6](
    vec3(0.08, 0.10, 0.08),
    vec3(0.14, 0.18, 0.12),
    vec3(0.22, 0.29, 0.17),
    vec3(0.34, 0.44, 0.22),
    vec3(0.50, 0.62, 0.27),
    vec3(0.72, 0.77, 0.34)
);
const vec3 cathedralArcadeFloorPalette[6] = vec3[6](
    vec3(0.08, 0.08, 0.09),
    vec3(0.14, 0.13, 0.12),
    vec3(0.21, 0.20, 0.18),
    vec3(0.30, 0.28, 0.25),
    vec3(0.42, 0.39, 0.34),
    vec3(0.58, 0.53, 0.45)
);
const float materialFloorMarker = (6.0 + 0.5) / 8.0;

const int bayer8[64] = int[64](
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21
);

float bayerValue(ivec2 p) {
    int idx = (p.x % 8) + (p.y % 8) * 8;
    return float(bayer8[idx]) / 64.0;
}

float saturationOf(vec3 color) {
    float maxChannel = max(max(color.r, color.g), color.b);
    float minChannel = min(min(color.r, color.g), color.b);
    return maxChannel - minChannel;
}

vec3 inkSwatch(int variant, int index) {
    if (variant == 1) {
        return dungeonInkPalette[index];
    }
    if (variant == 2) {
        return meadowInkPalette[index];
    }
    if (variant == 3) {
        return duskInkPalette[index];
    }
    if (variant == 4) {
        return arcaneInkPalette[index];
    }
    if (variant == 5) {
        return cathedralArcadeInkPalette[index];
    }
    return neutralInkPalette[index];
}

vec3 floorSwatch(int variant, int index) {
    if (variant == 1) {
        return dungeonFloorPalette[index];
    }
    if (variant == 2) {
        return meadowFloorPalette[index];
    }
    if (variant == 3) {
        return duskFloorPalette[index];
    }
    if (variant == 4) {
        return arcaneFloorPalette[index];
    }
    if (variant == 5) {
        return cathedralArcadeFloorPalette[index];
    }
    return neutralFloorPalette[index];
}

vec3 pickInkColor(vec3 sourceColor) {
    int variant = clamp(uPaletteVariant, 0, 5);
    float sourceLuma = dot(sourceColor, lumaWeights);
    float sourceSaturation = saturationOf(sourceColor);
    int bestIndex = 0;
    float bestScore = 1e9;

    for (int i = 0; i < 15; ++i) {
        vec3 paletteColor = inkSwatch(variant, i);
        vec3 diff = sourceColor - paletteColor;
        float rgbScore = dot(diff * diff, vec3(0.90, 1.10, 1.00));
        float lumaDelta = abs(sourceLuma - dot(paletteColor, lumaWeights));
        float satDelta = abs(sourceSaturation - saturationOf(paletteColor));
        float score = rgbScore + lumaDelta * 0.14 + satDelta * 0.10;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return inkSwatch(variant, bestIndex);
}

vec3 pickFloorInkColor(vec3 sourceColor) {
    int variant = clamp(uPaletteVariant, 0, 5);
    int bestIndex = 0;
    float bestScore = 1e9;

    for (int i = 0; i < 6; ++i) {
        vec3 paletteColor = floorSwatch(variant, i);
        vec3 diff = sourceColor - paletteColor;
        float score = dot(diff * diff, vec3(1.00, 0.90, 0.85));
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return floorSwatch(variant, bestIndex);
}

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - z * (uFarPlane - uNearPlane));
}

float sobelDepth(vec2 uv, vec2 texelSize) {
    float tl = linearizeDepth(texture(sceneDepth, uv + vec2(-1, 1) * texelSize).r);
    float t  = linearizeDepth(texture(sceneDepth, uv + vec2( 0, 1) * texelSize).r);
    float tr = linearizeDepth(texture(sceneDepth, uv + vec2( 1, 1) * texelSize).r);
    float l  = linearizeDepth(texture(sceneDepth, uv + vec2(-1, 0) * texelSize).r);
    float r  = linearizeDepth(texture(sceneDepth, uv + vec2( 1, 0) * texelSize).r);
    float bl = linearizeDepth(texture(sceneDepth, uv + vec2(-1,-1) * texelSize).r);
    float b  = linearizeDepth(texture(sceneDepth, uv + vec2( 0,-1) * texelSize).r);
    float br = linearizeDepth(texture(sceneDepth, uv + vec2( 1,-1) * texelSize).r);

    float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    float gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
    return sqrt(gx * gx + gy * gy);
}

float sobelNormal(vec2 uv, vec2 texelSize) {
    vec3 tl = texture(sceneNormal, uv + vec2(-1, 1) * texelSize).rgb;
    vec3 t  = texture(sceneNormal, uv + vec2( 0, 1) * texelSize).rgb;
    vec3 tr = texture(sceneNormal, uv + vec2( 1, 1) * texelSize).rgb;
    vec3 l  = texture(sceneNormal, uv + vec2(-1, 0) * texelSize).rgb;
    vec3 r  = texture(sceneNormal, uv + vec2( 1, 0) * texelSize).rgb;
    vec3 bl = texture(sceneNormal, uv + vec2(-1,-1) * texelSize).rgb;
    vec3 b  = texture(sceneNormal, uv + vec2( 0,-1) * texelSize).rgb;
    vec3 br = texture(sceneNormal, uv + vec2( 1,-1) * texelSize).rgb;

    vec3 gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    vec3 gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
    return length(gx) + length(gy);
}

void main() {
    vec2 texSize = vec2(textureSize(compositeColor, 0));
    vec2 texelSize = 1.0 / texSize;
    vec3 gradedColor = texture(compositeColor, vTexCoord).rgb;
    vec4 normalSample = texture(sceneNormal, vTexCoord);
    float rawDepth = texture(sceneDepth, vTexCoord).r;
    float linDepth = linearizeDepth(rawDepth);

    if (uDebugViewMode == 1) {
        fragColor = vec4(texture(sceneColor, vTexCoord).rgb, 1.0);
        return;
    }
    if (uDebugViewMode == 2) {
        fragColor = vec4(normalSample.rgb, 1.0);
        return;
    }
    if (uDebugViewMode == 3) {
        float depthPreview = 1.0 - exp(-linDepth * uDepthViewScale);
        fragColor = vec4(vec3(clamp(depthPreview, 0.0, 1.0)), 1.0);
        return;
    }

    bool isFloor = abs(normalSample.a - materialFloorMarker) < 0.02;
    vec2 patternScale = (uPatternScale <= 1.0) ? texSize : vec2(uPatternScale);
    float threshold = bayerValue(ivec2(vTexCoord * patternScale)) + uThresholdBias;

    float depthEdge = sobelDepth(vTexCoord, texelSize);
    float normalEdge = sobelNormal(vTexCoord, texelSize);
    float relativeDepthEdge = depthEdge / max(linDepth, 0.1);
    float edgeRaw = normalEdge * 0.4 + relativeDepthEdge * 0.1;
    float isEdge = 0.0;
    if (uEnableEdges != 0) {
        isEdge = (uEdgeThreshold >= 1.0) ? 0.0 : step(uEdgeThreshold, edgeRaw);
    }

    float luma = dot(gradedColor, lumaWeights);
    float shapedLuma = clamp((luma - 0.006) * 1.16, 0.0, 1.0);
    shapedLuma = shapedLuma * shapedLuma * (3.0 - 2.0 * shapedLuma);
    vec3 posterColor = gradedColor * (shapedLuma / max(luma, 0.001));
    posterColor = mix(vec3(shapedLuma), posterColor, 0.92);
    posterColor = clamp(posterColor, 0.0, 1.0);
    vec3 shadowInk = isFloor
        ? pickFloorInkColor(clamp(posterColor * 0.52 + vec3(0.01), 0.0, 1.0))
        : pickInkColor(clamp(posterColor * 0.52 + vec3(0.01), 0.0, 1.0));
    vec3 midInk = isFloor
        ? pickFloorInkColor(clamp(posterColor * 0.88 + vec3(0.02), 0.0, 1.0))
        : pickInkColor(clamp(posterColor * 0.88 + vec3(0.02), 0.0, 1.0));
    vec3 lightInk = isFloor
        ? pickFloorInkColor(clamp(posterColor * 1.20 + vec3(0.04), 0.0, 1.0))
        : pickInkColor(clamp(posterColor * 1.20 + vec3(0.04), 0.0, 1.0));

    float depthFade = 1.0 - smoothstep(uFogStart, uFogStart + 16.0, linDepth) * 0.72;

    if (uEnableDither == 0) {
        vec3 rawOutput = posterColor * (1.0 - isEdge * depthFade * 0.78);
        fragColor = vec4(rawOutput, 1.0);
        return;
    }

    float shade = clamp(shapedLuma * 2.2, 0.0, 2.0);
    vec3 inkColor = vec3(0.0);
    if (shade < 1.0) {
        inkColor = mix(shadowInk, midInk, step(threshold, shade));
    } else {
        inkColor = mix(midInk, lightInk, step(threshold, shade - 1.0));
    }
    inkColor = mix(inkColor, shadowInk * 0.28, isEdge * depthFade);
    fragColor = vec4(inkColor, 1.0);
}
