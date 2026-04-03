#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform vec3 uLightDirection;
uniform float uShadowCasterOffset;
uniform float uShadowNormalOffset;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vec3 worldNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
    worldPos.xyz += worldNormal * uShadowNormalOffset;
    worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
    gl_Position = worldPos;
}
