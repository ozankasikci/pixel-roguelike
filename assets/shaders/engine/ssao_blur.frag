#version 410 core

uniform sampler2D uSsaoInput;
uniform sampler2D uDepthTex;

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(uSsaoInput, 0));
    float centerDepth = texture(uDepthTex, vTexCoord).r;
    float result = 0.0;
    float totalWeight = 0.0;
    for (int x = -2; x < 2; ++x) {
        for (int y = -2; y < 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            vec2 sampleUv = vTexCoord + offset;
            float sampleDepth = texture(uDepthTex, sampleUv).r;
            // Depth-aware weight: reject samples across depth discontinuities
            float depthDiff = abs(centerDepth - sampleDepth);
            float weight = step(depthDiff, 0.001);
            result += texture(uSsaoInput, sampleUv).r * weight;
            totalWeight += weight;
        }
    }
    fragColor = vec4(vec3(result / max(totalWeight, 1.0)), 1.0);
}
