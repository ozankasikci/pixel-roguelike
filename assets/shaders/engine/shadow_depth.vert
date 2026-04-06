#version 410 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightViewProjection;
uniform vec3  uLightDirection;
uniform float uShadowCasterOffset;  // per-cascade, set from C++

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    if (uShadowCasterOffset > 0.0) {
        worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
    }
    gl_Position = uLightViewProjection * worldPos;
}
