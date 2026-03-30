#version 410 core

uniform sampler2D uSrcTex;
uniform vec2 uSrcTexelSize;

in vec2 vTexCoord;
out vec4 fragColor;

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

    fragColor = vec4(result, 1.0);
}
