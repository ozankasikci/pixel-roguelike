#version 410 core

uniform sampler2D compositeColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;

uniform int uEnableEdges;
uniform int uDebugViewMode;
uniform float uEdgeThreshold;
uniform float uDepthViewScale;
uniform float uFogStart;
uniform float uNearPlane;
uniform float uFarPlane;

in vec2 vTexCoord;
out vec4 fragColor;

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - z * (uFarPlane - uNearPlane));
}

float sobelDepth(vec2 uv, vec2 texelSize) {
    float tl = linearizeDepth(texture(sceneDepth, uv + vec2(-1, 1) * texelSize).r);
    float t  = linearizeDepth(texture(sceneDepth, uv + vec2( 0, 1) * texelSize).r);
    float tr = linearizeDepth(texture(sceneDepth, uv + vec2( 1, 1) * texelSize).r);
    float l  = linearizeDepth(texture(sceneDepth, uv + vec2(-1, 0) * texelSize).r);
    float r  = linearizeDepth(texture(sceneDepth, uv + vec2( 1, 0) * texelSize).r);
    float bl = linearizeDepth(texture(sceneDepth, uv + vec2(-1,-1) * texelSize).r);
    float b  = linearizeDepth(texture(sceneDepth, uv + vec2( 0,-1) * texelSize).r);
    float br = linearizeDepth(texture(sceneDepth, uv + vec2( 1,-1) * texelSize).r);

    float gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    float gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
    return sqrt(gx * gx + gy * gy);
}

float sobelNormal(vec2 uv, vec2 texelSize) {
    vec3 tl = texture(sceneNormal, uv + vec2(-1, 1) * texelSize).rgb;
    vec3 t  = texture(sceneNormal, uv + vec2( 0, 1) * texelSize).rgb;
    vec3 tr = texture(sceneNormal, uv + vec2( 1, 1) * texelSize).rgb;
    vec3 l  = texture(sceneNormal, uv + vec2(-1, 0) * texelSize).rgb;
    vec3 r  = texture(sceneNormal, uv + vec2( 1, 0) * texelSize).rgb;
    vec3 bl = texture(sceneNormal, uv + vec2(-1,-1) * texelSize).rgb;
    vec3 b  = texture(sceneNormal, uv + vec2( 0,-1) * texelSize).rgb;
    vec3 br = texture(sceneNormal, uv + vec2( 1,-1) * texelSize).rgb;

    vec3 gx = -tl - 2.0*l - bl + tr + 2.0*r + br;
    vec3 gy = -tl - 2.0*t - tr + bl + 2.0*b + br;
    return length(gx) + length(gy);
}

void main() {
    vec2 texSize = vec2(textureSize(compositeColor, 0));
    vec2 texelSize = 1.0 / texSize;
    vec3 gradedColor = texture(compositeColor, vTexCoord).rgb;
    vec4 normalSample = texture(sceneNormal, vTexCoord);
    float rawDepth = texture(sceneDepth, vTexCoord).r;
    float linDepth = linearizeDepth(rawDepth);
    bool isSkyPixel = rawDepth >= 0.9999;

    if (uDebugViewMode == 1) {
        fragColor = vec4(texture(sceneColor, vTexCoord).rgb, 1.0);
        return;
    }
    if (uDebugViewMode == 2) {
        fragColor = vec4(normalSample.rgb, 1.0);
        return;
    }
    if (uDebugViewMode == 3) {
        float depthPreview = 1.0 - exp(-linDepth * uDepthViewScale);
        fragColor = vec4(vec3(clamp(depthPreview, 0.0, 1.0)), 1.0);
        return;
    }
    if (uDebugViewMode == 4) {
        fragColor = vec4(gradedColor, 1.0);
        return;
    }

    if (isSkyPixel) {
        fragColor = vec4(gradedColor, 1.0);
        return;
    }

    vec3 result = gradedColor;
    if (uEnableEdges != 0 && uEdgeThreshold < 1.0) {
        float depthEdge = sobelDepth(vTexCoord, texelSize);
        float normalEdge = sobelNormal(vTexCoord, texelSize);
        float relativeDepthEdge = depthEdge / max(linDepth, 0.1);
        float edgeRaw = normalEdge * 0.4 + relativeDepthEdge * 0.1;
        float isEdge = step(uEdgeThreshold, edgeRaw);
        float depthFade = 1.0 - smoothstep(uFogStart, uFogStart + 16.0, linDepth) * 0.72;
        result *= (1.0 - isEdge * depthFade * 0.6);
    }
    fragColor = vec4(result, 1.0);
}
