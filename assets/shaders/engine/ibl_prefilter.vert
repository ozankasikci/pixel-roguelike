#version 410 core

layout(location = 0) in vec3 aPos;

out vec3 vLocalDir;

uniform mat4 uViewProjection;

void main() {
    vLocalDir = aPos;
    gl_Position = uViewProjection * vec4(aPos, 1.0);
}
