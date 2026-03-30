#version 410 core

uniform sampler2D uSrcTex;
uniform float uFilterRadius;

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    float x = uFilterRadius;
    float y = uFilterRadius;
    vec2 uv = vTexCoord;

    // 3x3 tent filter upsample
    vec3 a = texture(uSrcTex, uv + vec2(-x,  y)).rgb;
    vec3 b = texture(uSrcTex, uv + vec2( 0,  y)).rgb * 2.0;
    vec3 c = texture(uSrcTex, uv + vec2( x,  y)).rgb;
    vec3 d = texture(uSrcTex, uv + vec2(-x,  0)).rgb * 2.0;
    vec3 e = texture(uSrcTex, uv + vec2( 0,  0)).rgb * 4.0;
    vec3 f = texture(uSrcTex, uv + vec2( x,  0)).rgb * 2.0;
    vec3 g = texture(uSrcTex, uv + vec2(-x, -y)).rgb;
    vec3 h = texture(uSrcTex, uv + vec2( 0, -y)).rgb * 2.0;
    vec3 i = texture(uSrcTex, uv + vec2( x, -y)).rgb;

    fragColor = vec4((a + b + c + d + e + f + g + h + i) / 16.0, 1.0);
}
