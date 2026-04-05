#version 410 core

layout(location = 0) in vec3 aPosition;

uniform mat4 uModel;
uniform mat4 uLightViewProjection;

// Shadow caster offset — MECHANISM 1 of the dual CSM junction gap fix.
// Pushes shadow-casting geometry toward the light during shadow map construction,
// closing coverage gaps at wall-CEILING junctions (grazing-angle wall top edges).
// Keep this value small (0.06 is correct); large values (>0.08) create bright
// triangle artifacts at wall-FLOOR junctions even with the receiver-side offset.
// MECHANISM 2 lives in scene.frag sampleCsmShadow() — the receiver-side normal offset.
// See debug session .planning/debug/csm-white-line-wall-edge.md for full history.
uniform vec3  uLightDirection;     // world-space light direction (points away from light source)
uniform float uShadowCasterOffset; // world-space units to push toward light (0.0 = disabled)

void main() {
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    if (uShadowCasterOffset > 0.0) {
        worldPos.xyz += normalize(-uLightDirection) * uShadowCasterOffset;
    }
    gl_Position = uLightViewProjection * worldPos;
}
