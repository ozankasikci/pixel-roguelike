#version 410 core

uniform sampler2D sceneColor;
uniform mat4 uInverseView;
uniform float uThresholdBias;
uniform float uPatternScale;  // default 256.0, ImGui-tunable later

in vec2 vTexCoord;
out vec4 fragColor;

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

vec2 sphereUV(vec3 dir) {
    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159265);
    float v = 0.5 - asin(clamp(dir.y, -1.0, 1.0)) / 3.14159265;
    return vec2(u, v);
}

void main() {
    // Reconstruct view direction from screen UV
    vec2 ndc = vTexCoord * 2.0 - 1.0;
    vec4 viewDir4 = uInverseView * vec4(ndc, 0.0, 0.0);
    vec3 worldDir = normalize(viewDir4.xyz);

    // Map sphere UV to Bayer index (scale controls pattern density)
    vec2 suv = sphereUV(worldDir);
    ivec2 bayerCoord = ivec2(suv * uPatternScale);

    float threshold = bayerValue(bayerCoord) + uThresholdBias;

    // Sample linear-space luminance (D-03: compare in linear space)
    vec3 linearColor = texture(sceneColor, vTexCoord).rgb;
    float luma = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));  // Rec.709

    // Pure 1-bit output (no grayscale, no color)
    float bit = step(threshold, luma);
    fragColor = vec4(vec3(bit), 1.0);
}
