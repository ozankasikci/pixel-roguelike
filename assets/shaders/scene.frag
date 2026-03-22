#version 410 core

in vec3 vWorldPos;
in vec3 vNormal;

out vec4 fragColor;

void main() {
    // Simple directional light for testing dither visibility
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float NdotL = max(dot(normalize(vNormal), lightDir), 0.0);
    float luminance = 0.15 + 0.85 * NdotL;  // ambient + diffuse
    fragColor = vec4(vec3(luminance), 1.0);
}
