#version 410 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel;
uniform vec3 uLightDirection;
uniform float uShadowCasterOffset;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
    gl_Position = worldPos;
}
