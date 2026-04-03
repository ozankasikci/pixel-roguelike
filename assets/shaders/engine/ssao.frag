#version 410 core

uniform sampler2D uDepthTex;
uniform sampler2D uGeomNormalTex;
uniform sampler2D uNoiseTex;
uniform vec3 uSamples[32];
uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uInvProjection;
uniform vec2 uNoiseScale;
uniform float uAoRadius;
uniform float uAoBias;
uniform float uAoStrength;
uniform float uAoFadeStart;
uniform float uAoFadeEnd;

in vec2 vTexCoord;
out vec4 fragColor;

vec3 reconstructViewPos(vec2 texCoord, float depth) {
    vec4 ndc = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uInvProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth = texture(uDepthTex, vTexCoord).r;
    if (depth >= 0.9999) {
        fragColor = vec4(1.0);
        return;
    }

    vec3 viewPos = reconstructViewPos(vTexCoord, depth);
    float viewDistance = abs(viewPos.z);
    vec3 worldNormal = texture(uGeomNormalTex, vTexCoord).rgb * 2.0 - 1.0;
    vec3 normal = normalize(mat3(uView) * worldNormal);

    vec3 randomVec = normalize(texture(uNoiseTex, vTexCoord * uNoiseScale).xyz);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < 32; ++i) {
        vec3 samplePos = viewPos + TBN * uSamples[i] * uAoRadius;
        vec4 offset = uProjection * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        float sampleDepth = reconstructViewPos(offset.xy, texture(uDepthTex, offset.xy).r).z;
        float rangeCheck = smoothstep(0.0, 1.0, uAoRadius / abs(viewPos.z - sampleDepth));
        occlusion += (sampleDepth >= samplePos.z + uAoBias) ? 0.0 : 1.0 * rangeCheck;
    }

    float fade = 1.0 - smoothstep(uAoFadeStart, uAoFadeEnd, viewDistance);
    fragColor = vec4(vec3(1.0 - (occlusion / 32.0) * uAoStrength * fade), 1.0);
}
