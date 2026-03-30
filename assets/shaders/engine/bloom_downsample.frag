#version 410 core

uniform sampler2D uSrcTex;
uniform vec2 uSrcTexelSize;
uniform float uThreshold;    // brightness threshold (0 = no filtering)
uniform float uSoftKnee;     // soft knee width (typically 0.5)

in vec2 vTexCoord;
out vec4 fragColor;

// Soft-knee threshold: smoothly fades contribution around the threshold
// instead of a hard cutoff, preventing harsh bloom edges
vec3 prefilter(vec3 color) {
    float brightness = max(color.r, max(color.g, color.b));
    float knee = uThreshold * uSoftKnee;
    float soft = brightness - uThreshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - uThreshold) / max(brightness, 0.00001);
    return color * max(contribution, 0.0);
}

void main() {
    vec2 uv = vTexCoord;
    vec2 ts = uSrcTexelSize;

    // 13-tap downsample kernel (COD: Advanced Warfare technique)
    vec3 a = texture(uSrcTex, uv + vec2(-2.0, -2.0) * ts).rgb;
    vec3 b = texture(uSrcTex, uv + vec2( 0.0, -2.0) * ts).rgb;
    vec3 c = texture(uSrcTex, uv + vec2( 2.0, -2.0) * ts).rgb;
    vec3 d = texture(uSrcTex, uv + vec2(-2.0,  0.0) * ts).rgb;
    vec3 e = texture(uSrcTex, uv + vec2( 0.0,  0.0) * ts).rgb;
    vec3 f = texture(uSrcTex, uv + vec2( 2.0,  0.0) * ts).rgb;
    vec3 g = texture(uSrcTex, uv + vec2(-2.0,  2.0) * ts).rgb;
    vec3 h = texture(uSrcTex, uv + vec2( 0.0,  2.0) * ts).rgb;
    vec3 i = texture(uSrcTex, uv + vec2( 2.0,  2.0) * ts).rgb;
    vec3 j = texture(uSrcTex, uv + vec2(-1.0, -1.0) * ts).rgb;
    vec3 k = texture(uSrcTex, uv + vec2( 1.0, -1.0) * ts).rgb;
    vec3 l = texture(uSrcTex, uv + vec2(-1.0,  1.0) * ts).rgb;
    vec3 m = texture(uSrcTex, uv + vec2( 1.0,  1.0) * ts).rgb;

    vec3 result = e * 0.125
                + (a + c + g + i) * 0.03125
                + (b + d + f + h) * 0.0625
                + (j + k + l + m) * 0.125;

    // Apply threshold prefilter on first pass (uThreshold > 0)
    if (uThreshold > 0.0) {
        result = prefilter(result);
    }

    fragColor = vec4(result, 1.0);
}
