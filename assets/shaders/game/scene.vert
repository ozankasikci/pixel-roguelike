#version 410 core

layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uBaseColor;
uniform int uMaterialKind;
uniform float uTimeSeconds;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vLocalPos;
out vec2 vTexCoord;
out vec3 vObjectPos;
out vec3 vWorldTangent;

const int MATERIAL_WAX = 3;

float saturationOf(vec3 color) {
    float maxChannel = max(max(color.r, color.g), color.b);
    float minChannel = min(min(color.r, color.g), color.b);
    return maxChannel - minChannel;
}

void main() {
    vec3 localPos = aPosition;
    float flameMask = (uMaterialKind == MATERIAL_WAX && saturationOf(uBaseColor) > 0.28 && uBaseColor.r > 0.88) ? 1.0 : 0.0;
    if (flameMask > 0.0) {
        float tip = clamp(aPosition.y + 0.5, 0.0, 1.0);
        float phase = uTimeSeconds * 8.4 + dot(uModel[3].xz, vec2(1.7, 2.3));
        float widthPulse = 0.94 - 0.10 * tip + 0.05 * sin(phase * 1.6 + tip * 3.4);
        float heightPulse = 1.00 + 0.10 * tip + 0.07 * sin(phase * 2.2 + 0.7);
        localPos.x *= mix(1.0, widthPulse, flameMask);
        localPos.z *= mix(1.0, widthPulse, flameMask);
        localPos.y *= mix(1.0, heightPulse, flameMask);
        localPos.x += flameMask * tip * tip * (0.06 * sin(phase + tip * 2.8));
        localPos.z += flameMask * tip * tip * (0.05 * cos(phase * 1.3 + tip * 3.6));
        localPos.y += flameMask * tip * 0.05 * sin(phase * 2.7 + 1.1);
    }

    vec4 worldPos = uModel * vec4(localPos, 1.0);
    vec3 objectScale = vec3(length(uModel[0].xyz), length(uModel[1].xyz), length(uModel[2].xyz));
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vLocalPos = localPos;
    vTexCoord = aTexCoord;
    vObjectPos = localPos * objectScale;
    vWorldTangent = normalize(mat3(uModel) * aTangent);
    gl_Position = uProjection * uView * worldPos;
}
