#version 410 core

uniform sampler2D compositeColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;

uniform int uEnableDither;
uniform int uEnableEdges;
uniform int uDebugViewMode;
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
const vec3 inkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.06, 0.05, 0.05),
    vec3(0.12, 0.11, 0.11),
    vec3(0.20, 0.19, 0.19),
    vec3(0.30, 0.29, 0.28),
    vec3(0.42, 0.41, 0.40),
    vec3(0.56, 0.55, 0.53),
    vec3(0.70, 0.69, 0.66),
    vec3(0.84, 0.83, 0.80),
    vec3(0.96, 0.95, 0.90),
    vec3(0.33, 0.13, 0.10),
    vec3(0.49, 0.20, 0.15),
    vec3(0.65, 0.28, 0.19),
    vec3(0.64, 0.55, 0.30),
    vec3(0.80, 0.71, 0.36)
);
const vec3 floorInkPalette[6] = vec3[6](
    vec3(0.18, 0.06, 0.05),
    vec3(0.29, 0.10, 0.08),
    vec3(0.40, 0.15, 0.11),
    vec3(0.52, 0.21, 0.15),
    vec3(0.64, 0.29, 0.19),
    vec3(0.78, 0.39, 0.24)
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

vec3 pickInkColor(vec3 sourceColor) {
    float sourceLuma = dot(sourceColor, lumaWeights);
    float sourceSaturation = saturationOf(sourceColor);
    int bestIndex = 0;
    float bestScore = 1e9;

    for (int i = 0; i < 15; ++i) {
        vec3 diff = sourceColor - inkPalette[i];
        float rgbScore = dot(diff * diff, vec3(0.90, 1.10, 1.00));
        float lumaDelta = abs(sourceLuma - dot(inkPalette[i], lumaWeights));
        float satDelta = abs(sourceSaturation - saturationOf(inkPalette[i]));
        float score = rgbScore + lumaDelta * 0.14 + satDelta * 0.10;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return inkPalette[bestIndex];
}

vec3 pickFloorInkColor(vec3 sourceColor) {
    int bestIndex = 0;
    float bestScore = 1e9;

    for (int i = 0; i < 6; ++i) {
        vec3 diff = sourceColor - floorInkPalette[i];
        float score = dot(diff * diff, vec3(1.00, 0.90, 0.85));
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return floorInkPalette[bestIndex];
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
    float shapedLuma = clamp((luma - 0.010) * 1.10, 0.0, 1.0);
    shapedLuma = shapedLuma * shapedLuma * (3.0 - 2.0 * shapedLuma);
    vec3 posterColor = gradedColor * (shapedLuma / max(luma, 0.001));
    posterColor = mix(vec3(shapedLuma), posterColor, 0.92);
    posterColor = clamp(posterColor, 0.0, 1.0);
    vec3 inkColor = isFloor ? pickFloorInkColor(posterColor) : pickInkColor(posterColor);

    float depthFade = 1.0 - smoothstep(uFogStart, uFogStart + 16.0, linDepth) * 0.72;

    if (uEnableDither == 0) {
        vec3 rawOutput = posterColor * (1.0 - isEdge * depthFade);
        fragColor = vec4(rawOutput, 1.0);
        return;
    }

    float bit = step(threshold, shapedLuma);
    bit = bit * (1.0 - isEdge * depthFade);
    fragColor = vec4(inkColor * bit, 1.0);
}
