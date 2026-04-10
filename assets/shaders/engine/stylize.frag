#version 410 core

uniform sampler2D compositeColor;
uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;

uniform int uEnableEdges;
uniform int uEnableFxaa;
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

vec3 sampleFxaa(vec2 uv, vec2 texelSize) {
    vec3 rgbM  = texture(compositeColor, uv).rgb;
    vec3 rgbNW = texture(compositeColor, uv + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 rgbNE = texture(compositeColor, uv + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 rgbSW = texture(compositeColor, uv + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 rgbSE = texture(compositeColor, uv + vec2( 1.0,  1.0) * texelSize).rgb;

    const vec3 lumaWeights = vec3(0.299, 0.587, 0.114);
    float lumaM  = dot(rgbM,  lumaWeights);
    float lumaNW = dot(rgbNW, lumaWeights);
    float lumaNE = dot(rgbNE, lumaWeights);
    float lumaSW = dot(rgbSW, lumaWeights);
    float lumaSE = dot(rgbSE, lumaWeights);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.0312, lumaMax * 0.125)) {
        return rgbM;
    }

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * 0.125), 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * texelSize;

    vec3 rgbA = 0.5 * (
        texture(compositeColor, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(compositeColor, uv + dir * (2.0 / 3.0 - 0.5)).rgb
    );
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(compositeColor, uv + dir * -0.5).rgb +
        texture(compositeColor, uv + dir * 0.5).rgb
    );

    float lumaB = dot(rgbB, lumaWeights);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        return rgbA;
    }
    return rgbB;
}

void main() {
    vec2 texSize = vec2(textureSize(compositeColor, 0));
    vec2 texelSize = 1.0 / texSize;
    vec3 gradedColor = (uEnableFxaa != 0) ? sampleFxaa(vTexCoord, texelSize)
                                          : texture(compositeColor, vTexCoord).rgb;
    vec4 normalSample = texture(sceneNormal, vTexCoord);
    float rawDepth = texture(sceneDepth, vTexCoord).r;
    float linDepth = linearizeDepth(rawDepth);
    bool isSkyPixel = rawDepth >= 0.9999;

    if (uDebugViewMode == 1 || uDebugViewMode >= 5) {
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
