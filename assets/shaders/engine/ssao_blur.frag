#version 410 core

uniform sampler2D uSsaoInput;

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSsaoInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(uSsaoInput, vTexCoord + offset).r;
        }
    }
    fragColor = vec4(vec3(result / 16.0), 1.0);
}
