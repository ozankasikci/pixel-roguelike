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
    float luma = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));

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
        fogFactor = 1.0 - exp(-uFogDensity * fogDepth);
        fogFactor = clamp(fogFactor, 0.0, 1.0);
    }

    // Apply fog: darken distant geometry
    luma = luma * (1.0 - fogFactor);

    // ---- 1-bit dithering ----
    float bit = step(threshold, luma);

    // Edges are always black (creates the architectural outline effect)
    bit = bit * (1.0 - isEdge);

    fragColor = vec4(vec3(bit), 1.0);
}
