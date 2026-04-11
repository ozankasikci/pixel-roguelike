#version 410 core

layout(location = 0) in vec2 aQuadPos;
layout(location = 1) in vec3 aWorldPos;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aSize;
layout(location = 4) in float aRotation;
layout(location = 5) in float aNormalizedAge;

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;
out float vLinearDepth;

void main() {
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    float cosR = cos(aRotation);
    float sinR = sin(aRotation);
    vec2 rotated = vec2(
        aQuadPos.x * cosR - aQuadPos.y * sinR,
        aQuadPos.x * sinR + aQuadPos.y * cosR
    );

    vec3 worldPos = aWorldPos
                  + cameraRight * rotated.x * aSize
                  + cameraUp    * rotated.y * aSize;

    vec4 viewPos = uView * vec4(worldPos, 1.0);
    gl_Position = uProjection * viewPos;

    vUV = aQuadPos + 0.5;
    vColor = aColor;
    vLinearDepth = -viewPos.z;
}
