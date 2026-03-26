#version 410 core

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;

uniform float uThresholdBias;
uniform float uPatternScale;

// Post-process controls
uniform float uEdgeThreshold;   // 0.0 = no edges, 0.1 = subtle, 0.5 = heavy
uniform float uFogDensity;      // exponential fog density (0.0 = off)
uniform float uFogStart;        // linear depth where fog begins
uniform float uNearPlane;
uniform float uFarPlane;

in vec2 vTexCoord;
out vec4 fragColor;

const vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);
const vec3 fogColor = vec3(0.10, 0.10, 0.10);
const vec3 inkPalette[15] = vec3[15](
    vec3(0.00, 0.00, 0.00),
    vec3(0.02, 0.02, 0.02),
    vec3(0.05, 0.05, 0.05),
    vec3(0.09, 0.09, 0.09),
    vec3(0.15, 0.15, 0.15),
    vec3(0.23, 0.23, 0.23),
    vec3(0.33, 0.33, 0.33),
    vec3(0.45, 0.45, 0.45),
    vec3(0.56, 0.56, 0.56),
    vec3(0.66, 0.66, 0.66),
    vec3(0.75, 0.75, 0.75),
    vec3(0.83, 0.83, 0.83),
    vec3(0.90, 0.90, 0.90),
    vec3(0.96, 0.96, 0.96),
    vec3(1.00, 1.00, 1.00)
);

// ---- 8x8 Bayer matrix ----
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

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    float x1 = mix(a, b, f.x);
    float x2 = mix(c, d, f.x);
    return mix(x1, x2, f.y);
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
        float score = rgbScore + lumaDelta * 0.16 + satDelta * 0.18;
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    return inkPalette[bestIndex];
}

// Convert depth buffer value to linear depth
float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0; // NDC
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - z * (uFarPlane - uNearPlane));
}

// ---- Sobel edge detection on depth ----
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

    return sqrt(gx*gx + gy*gy);
}

// ---- Sobel edge detection on normals ----
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
    vec2 texSize = vec2(textureSize(sceneColor, 0));
    vec2 texelSize = 1.0 / texSize;

    // ---- Screen-space Bayer dithering at internal resolution ----
    // Use pixel coordinates in the internal FBO — gives a clean regular grid.
    // At 480p with nearest-neighbor upscale, cells are chunky and pattern is subtle.
    ivec2 bayerCoord = ivec2(vTexCoord * texSize);
    float threshold = bayerValue(bayerCoord) + uThresholdBias;

    // ---- Sample scene luminance ----
    vec3 linearColor = texture(sceneColor, vTexCoord).rgb;

    // ---- Linearize depth (used by edges + fog) ----
    float rawDepth = texture(sceneDepth, vTexCoord).r;
    float linDepth = linearizeDepth(rawDepth);

    // ---- Edge detection (Sobel on depth + normals) ----
    float depthEdge  = sobelDepth(vTexCoord, texelSize);
    float normalEdge = sobelNormal(vTexCoord, texelSize);
    // Normal edges detect actual geometry outlines (pillar edges, arch curves).
    // Depth edges detect silhouettes but over-fire in dense corridors.
    // Blend: normals dominate, depth only for strong silhouettes.
    float relativeDepthEdge = depthEdge / max(linDepth, 0.1);
    float edgeRaw = normalEdge * 0.4 + relativeDepthEdge * 0.1;
    float isEdge = (uEdgeThreshold >= 1.0) ? 0.0 : step(uEdgeThreshold, edgeRaw);

    // ---- Depth fog (exponential) ----
    float fogFactor = 0.0;
    if (uFogDensity > 0.0) {
        float fogDepth = max(linDepth - uFogStart, 0.0);
        float rawFog = 1.0 - exp(-uFogDensity * fogDepth);
        rawFog = clamp(rawFog, 0.0, 1.0);
        float distanceFog = smoothstep(uFogStart, uFogStart + 16.0, linDepth);
        fogFactor = mix(rawFog, distanceFog, 0.35);
        fogFactor = fogFactor * fogFactor * (3.0 - 2.0 * fogFactor);
    }

    vec2 fogUv = vTexCoord * texSize;
    float fogNoiseLarge = valueNoise2(fogUv * 0.010 + vec2(linDepth * 0.09, linDepth * 0.04));
    float fogNoiseMedium = valueNoise2(fogUv * 0.022 + vec2(-linDepth * 0.06, linDepth * 0.05));
    float fogNoise = mix(fogNoiseLarge, fogNoiseMedium, 0.45);
    float fogShade = mix(0.88, 1.14, fogNoise);
    float verticalMist = smoothstep(0.92, 0.18, vTexCoord.y);
    float depthMist = smoothstep(uFogStart + 1.0, uFogStart + 20.0, linDepth);
    float mistAmount = depthMist * mix(0.78, 1.22, fogNoise) * mix(0.92, 1.08, verticalMist);
    mistAmount = clamp(mistAmount, 0.0, 1.0);

    vec3 nearFogColor = vec3(0.08, 0.08, 0.08);
    vec3 farFogColor = vec3(0.14, 0.14, 0.14);
    vec3 localFogColor = mix(nearFogColor, farFogColor, clamp(0.35 + fogNoise * 0.45 + verticalMist * 0.12, 0.0, 1.0));
    localFogColor = clamp(localFogColor * fogShade, 0.0, 1.0);

    float fogContrastFade = 1.0 - fogFactor * 0.55;
    linearColor = mix(vec3(dot(linearColor, lumaWeights)), linearColor, fogContrastFade);
    linearColor = mix(linearColor, localFogColor, fogFactor * mistAmount);

    float luma = dot(linearColor, lumaWeights);
    float shapedLuma = clamp((luma - 0.015) * 1.08, 0.0, 1.0);
    shapedLuma = shapedLuma * shapedLuma * (3.0 - 2.0 * shapedLuma);
    float fogLuma = dot(localFogColor, lumaWeights);
    shapedLuma = mix(shapedLuma, fogLuma, fogFactor * 0.34 * mistAmount);
    vec3 inkColor = pickInkColor(vec3(shapedLuma));

    // ---- 1-bit dithering ----
    float fogThreshold = mix(threshold, 0.46 + (fogNoise - 0.5) * 0.10, fogFactor * 0.30);
    float bit = step(fogThreshold, shapedLuma);

    // Edges are always black (creates the architectural outline effect)
    float edgeFade = 1.0 - fogFactor * 0.72;
    bit = bit * (1.0 - isEdge * edgeFade);

    fragColor = vec4(inkColor * bit, 1.0);
}
