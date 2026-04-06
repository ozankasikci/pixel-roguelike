#version 410 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightViewProjection;

// Shadow caster offset — pushes geometry toward the light to close coverage gaps
// at grazing-angle junctions (e.g. wall top edge meeting ceiling). Set to a
// non-zero value only for CSM (directional sun) passes; leave 0.0 for spot lights.
uniform vec3  uLightDirection;    // world-space light direction (points away from light source)
uniform float uShadowCasterOffset; // world-space units to push toward light (0.0 = disabled)

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    if (uShadowCasterOffset > 0.0) {
        worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
    }
    gl_Position = uLightViewProjection * worldPos;
}
