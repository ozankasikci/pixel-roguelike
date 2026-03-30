#version 410 core
layout(triangles, invocations = 3) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 uLightSpaceMatrices[3];

void main() {
    gl_Layer = gl_InvocationID;
    for (int i = 0; i < 3; ++i) {
        gl_Position = uLightSpaceMatrices[gl_InvocationID] * gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
