#version 410 core

in vec2 vUV;
in vec4 vColor;
in float vLinearDepth;

uniform sampler2D uParticleTexture;
uniform sampler2D uSceneDepth;
uniform int uHasTexture;
uniform float uEmissiveStrength;
uniform float uSoftFadeRange;
uniform float uNearPlane;
uniform float uFarPlane;
uniform vec2 uViewportSize;

layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d) {
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - (2.0 * d - 1.0) * (uFarPlane - uNearPlane));
}

void main() {
    vec4 texColor;
    if (uHasTexture != 0) {
        texColor = texture(uParticleTexture, vUV);
    } else {
        float dist = length(vUV - vec2(0.5));
        float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
        texColor = vec4(1.0, 1.0, 1.0, alpha);
    }

    vec4 color = texColor * vColor;

    if (uSoftFadeRange > 0.0) {
        vec2 screenUV = gl_FragCoord.xy / uViewportSize;
        float sceneDepthRaw = texture(uSceneDepth, screenUV).r;
        float sceneDepthLinear = linearizeDepth(sceneDepthRaw);
        float fade = clamp((sceneDepthLinear - vLinearDepth) / uSoftFadeRange, 0.0, 1.0);
        color.a *= fade;
    }

    color.rgb *= uEmissiveStrength;

    FragColor = color;
}
