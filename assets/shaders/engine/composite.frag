#version 410 core

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D sceneNormal;
uniform sampler2D uCloudLayerA;
uniform sampler2D uCloudLayerB;
uniform sampler2D uHorizonLayer;
uniform sampler2D uSkyPanorama;
uniform samplerCube uSkyCubemap;

uniform int uEnableSky;
uniform int uEnableFog;
uniform int uEnableToneMap;
uniform int uEnableBloom;
uniform int uEnableVignette;
uniform int uEnableGrain;
uniform int uEnableScanlines;
uniform int uEnableSharpen;
uniform int uToneMapMode;
uniform int uDebugViewMode;
uniform int uMoonEnabled;
uniform int uHasCloudLayerA;
uniform int uHasCloudLayerB;
uniform int uHasHorizonLayer;
uniform int uHasSkyPanorama;
uniform int uHasSkyCubemap;

uniform float uFogDensity;
uniform float uFogStart;
uniform float uDepthViewScale;
uniform float uExposure;
uniform float uGamma;
uniform float uContrast;
uniform float uSaturation;
uniform float uBloomThreshold;
uniform float uBloomIntensity;
uniform float uBloomRadius;
uniform float uVignetteStrength;
uniform float uVignetteSoftness;
uniform float uGrainAmount;
uniform float uScanlineAmount;
uniform float uScanlineDensity;
uniform float uSharpenAmount;
uniform float uSplitToneStrength;
uniform float uSplitToneBalance;
uniform float uTimeSeconds;
uniform float uNearPlane;
uniform float uFarPlane;
uniform float uSunSize;
uniform float uSunGlow;
uniform float uSkyPanoramaStrength;
uniform float uSkyPanoramaYawOffset;
uniform float uSkyCubemapStrength;
uniform float uMoonSize;
uniform float uMoonGlow;
uniform float uCloudScale;
uniform float uCloudSpeed;
uniform float uCloudCoverage;
uniform float uCloudParallax;
uniform float uHorizonHeight;
uniform float uHorizonFade;

uniform vec3 uFogNearColor;
uniform vec3 uFogFarColor;
uniform vec3 uShadowTint;
uniform vec3 uHighlightTint;
uniform vec3 uSkyZenithColor;
uniform vec3 uSkyHorizonColor;
uniform vec3 uSkyGroundHazeColor;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform vec3 uSkyPanoramaTint;
uniform vec3 uSkyCubemapTint;
uniform vec3 uMoonDirection;
uniform vec3 uMoonColor;
uniform vec3 uCloudTint;
uniform vec3 uHorizonTint;
uniform mat4 uInverseViewProjection;

in vec2 vTexCoord;
out vec4 fragColor;

const vec3 lumaWeights = vec3(0.2126, 0.7152, 0.0722);
const float PI = 3.14159265359;

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise2(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash12(i);
    float b = hash12(i + vec2(1.0, 0.0));
    float c = hash12(i + vec2(0.0, 1.0));
    float d = hash12(i + vec2(1.0, 1.0));

    float x1 = mix(a, b, f.x);
    float x2 = mix(c, d, f.x);
    return mix(x1, x2, f.y);
}

vec3 acesFitted(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 applySplitTone(vec3 color) {
    float luma = dot(color, lumaWeights);
    float shadowMask = 1.0 - smoothstep(uSplitToneBalance - 0.25, uSplitToneBalance + 0.05, luma);
    float highlightMask = smoothstep(uSplitToneBalance - 0.05, uSplitToneBalance + 0.25, luma);
    vec3 shadowed = mix(color, color * uShadowTint, uSplitToneStrength * shadowMask);
    return mix(shadowed, shadowed * uHighlightTint, uSplitToneStrength * highlightMask);
}

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - z * (uFarPlane - uNearPlane));
}

vec3 sampleSceneColor(vec2 uv) {
    return texture(sceneColor, uv).rgb;
}

vec3 sharpenScene(vec2 uv, vec2 texelSize) {
    vec3 center = sampleSceneColor(uv);
    vec3 neighbors = sampleSceneColor(uv + vec2(texelSize.x, 0.0))
        + sampleSceneColor(uv - vec2(texelSize.x, 0.0))
        + sampleSceneColor(uv + vec2(0.0, texelSize.y))
        + sampleSceneColor(uv - vec2(0.0, texelSize.y));
    vec3 blurred = neighbors * 0.25;
    return max(center + (center - blurred) * uSharpenAmount, 0.0);
}

vec3 bloomGlow(vec2 uv, vec2 texelSize) {
    vec2 radius = texelSize * max(uBloomRadius, 0.0);
    vec2 offsets[8] = vec2[8](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.7,  0.7), vec2(-0.7,  0.7),
        vec2( 0.7, -0.7), vec2(-0.7, -0.7)
    );

    vec3 glow = vec3(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec3 sampleColor = sampleSceneColor(uv + offsets[i] * radius);
        float luma = dot(sampleColor, lumaWeights);
        float weight = smoothstep(uBloomThreshold, 1.2, luma);
        glow += sampleColor * weight;
        totalWeight += weight;
    }

    return totalWeight > 0.0 ? glow / totalWeight : vec3(0.0);
}

vec3 reconstructWorldDir(vec2 uv) {
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 nearPoint = uInverseViewProjection * vec4(ndc, -1.0, 1.0);
    vec4 farPoint = uInverseViewProjection * vec4(ndc, 1.0, 1.0);
    nearPoint /= max(nearPoint.w, 0.0001);
    farPoint /= max(farPoint.w, 0.0001);
    return normalize(farPoint.xyz - nearPoint.xyz);
}

vec2 skyUvFromDirection(vec3 dir) {
    float longitude = atan(dir.z, dir.x) / (2.0 * PI) + 0.5;
    float latitude = asin(clamp(dir.y, -1.0, 1.0)) / PI + 0.5;
    return vec2(fract(longitude), clamp(latitude, 0.0, 1.0));
}

float celestialDisc(vec3 rayDir, vec3 lightDir, float angularRadius, float softness) {
    float angle = acos(clamp(dot(normalize(rayDir), normalize(lightDir)), -1.0, 1.0));
    return 1.0 - smoothstep(angularRadius, angularRadius + softness, angle);
}

vec4 samplePanoramaLayer(sampler2D layer, vec3 worldDir, float xShift, float yShift, float scale) {
    vec2 uv = skyUvFromDirection(worldDir);
    uv.x = fract(uv.x * scale + xShift);
    uv.y = clamp((uv.y - 0.5) * scale + 0.5 + yShift, 0.0, 1.0);
    return texture(layer, uv);
}

vec3 renderSky(vec3 worldDir) {
    float horizonBlend = smoothstep(-0.22, 0.18, worldDir.y);
    float zenithBlend = smoothstep(0.02, 0.82, worldDir.y);

    vec3 skyColor = mix(uSkyGroundHazeColor, uSkyHorizonColor, horizonBlend);
    skyColor = mix(skyColor, uSkyZenithColor, zenithBlend);

    if (uHasSkyCubemap != 0) {
        vec3 cubemapColor = texture(uSkyCubemap, normalize(vec3(worldDir.x, worldDir.y, -worldDir.z))).rgb * uSkyCubemapTint;
        skyColor = mix(skyColor, cubemapColor, clamp(uSkyCubemapStrength, 0.0, 1.0));
    } else if (uHasSkyPanorama != 0) {
        vec2 panoUv = skyUvFromDirection(worldDir);
        panoUv.x = fract(panoUv.x + uSkyPanoramaYawOffset);
        vec3 panoramaColor = texture(uSkyPanorama, panoUv).rgb * uSkyPanoramaTint;
        skyColor = mix(skyColor, panoramaColor, clamp(uSkyPanoramaStrength, 0.0, 1.0));
    }

    float horizonGlow = exp(-abs(worldDir.y) * 10.0);
    skyColor = mix(skyColor, mix(uSkyGroundHazeColor, uSkyHorizonColor, 0.65), horizonGlow * 0.12);

    float sunDisc = celestialDisc(worldDir, normalize(uSunDirection), max(uSunSize, 0.001), max(uSunSize * 1.8, 0.002));
    float sunHalo = pow(max(dot(worldDir, normalize(uSunDirection)), 0.0), mix(220.0, 10.0, clamp(uSunGlow, 0.0, 1.0))) * uSunGlow;
    skyColor += uSunColor * (sunDisc * 1.35 + sunHalo * 0.55);

    if (uMoonEnabled != 0) {
        float moonDisc = celestialDisc(worldDir, normalize(uMoonDirection), max(uMoonSize, 0.001), max(uMoonSize * 1.5, 0.002));
        float moonHalo = pow(max(dot(worldDir, normalize(uMoonDirection)), 0.0), mix(180.0, 14.0, clamp(uMoonGlow, 0.0, 1.0))) * uMoonGlow;
        skyColor += uMoonColor * (moonDisc * 0.85 + moonHalo * 0.30);
    }

    float upperSkyMask = smoothstep(-0.04, 0.16, worldDir.y);
    float layerScaleA = max(uCloudScale, 0.001);
    float layerScaleB = max(uCloudScale * 1.55, 0.001);
    float coverageThreshold = mix(0.94, 0.16, clamp(uCloudCoverage, 0.0, 1.0));
    float parallax = worldDir.y * uCloudParallax * 0.12;

    if (uHasCloudLayerA != 0) {
        vec4 cloudsA = samplePanoramaLayer(
            uCloudLayerA,
            worldDir,
            uTimeSeconds * uCloudSpeed,
            parallax,
            layerScaleA
        );
        float cloudAlpha = smoothstep(coverageThreshold, 1.0, cloudsA.a) * upperSkyMask;
        vec3 cloudColor = mix(skyColor, cloudsA.rgb * uCloudTint, 0.72);
        skyColor = mix(skyColor, cloudColor, cloudAlpha * 0.82);
    }

    if (uHasCloudLayerB != 0) {
        vec4 cloudsB = samplePanoramaLayer(
            uCloudLayerB,
            worldDir,
            -uTimeSeconds * uCloudSpeed * 0.62,
            -parallax * 0.45,
            layerScaleB
        );
        float cloudAlpha = smoothstep(coverageThreshold + 0.06, 1.0, cloudsB.a) * upperSkyMask;
        vec3 cloudColor = mix(skyColor, cloudsB.rgb * uCloudTint, 0.62);
        skyColor = mix(skyColor, cloudColor, cloudAlpha * 0.58);
    }

    if (uHasHorizonLayer != 0) {
        vec4 horizon = texture(uHorizonLayer, skyUvFromDirection(worldDir));
        float horizonMask = 1.0 - smoothstep(uHorizonHeight, uHorizonHeight + max(uHorizonFade, 0.001), abs(worldDir.y));
        skyColor = mix(skyColor, horizon.rgb * uHorizonTint, horizon.a * horizonMask);
    }

    return max(skyColor, vec3(0.0));
}

void main() {
    vec2 texSize = vec2(textureSize(sceneColor, 0));
    vec2 texelSize = 1.0 / texSize;
    vec3 color = (uEnableSharpen != 0) ? sharpenScene(vTexCoord, texelSize) : sampleSceneColor(vTexCoord);

    float rawDepth = texture(sceneDepth, vTexCoord).r;
    float linDepth = linearizeDepth(rawDepth);
    bool isSkyPixel = (uEnableSky != 0) && rawDepth >= 0.9999;
    vec3 worldDir = reconstructWorldDir(vTexCoord);
    vec3 skyColor = (uEnableSky != 0) ? renderSky(worldDir) : uFogFarColor;

    if (uDebugViewMode == 4) {
        fragColor = vec4(skyColor, 1.0);
        return;
    }

    if (isSkyPixel) {
        color = skyColor;
    }

    float fogFactor = 0.0;
    if (uEnableFog != 0 && uFogDensity > 0.0) {
        float fogDepth = max(linDepth - uFogStart, 0.0);
        float rawFog = 1.0 - exp(-uFogDensity * fogDepth);
        rawFog = clamp(rawFog, 0.0, 1.0);
        float distanceFog = smoothstep(uFogStart, uFogStart + 16.0, linDepth);
        fogFactor = mix(rawFog, distanceFog, 0.35);
        fogFactor = fogFactor * fogFactor * (3.0 - 2.0 * fogFactor);
    }

    vec2 fogUv = vTexCoord * texSize;
    float fogNoiseLarge = valueNoise2(fogUv * 0.010 + vec2(linDepth * 0.09, linDepth * 0.04));
    float fogNoiseMedium = valueNoise2(fogUv * 0.022 + vec2(-linDepth * 0.06, linDepth * 0.05));
    float fogNoise = mix(fogNoiseLarge, fogNoiseMedium, 0.45);
    float fogShade = mix(0.92, 1.10, fogNoise);
    float verticalMist = smoothstep(0.92, 0.18, vTexCoord.y);
    float depthMist = smoothstep(uFogStart + 1.0, uFogStart + 20.0, linDepth);
    float mistAmount = depthMist * mix(0.78, 1.22, fogNoise) * mix(0.92, 1.08, verticalMist);
    mistAmount = clamp(mistAmount, 0.0, 1.0);

    vec3 localFogColor = mix(uFogNearColor, uFogFarColor, clamp(0.28 + fogNoise * 0.42 + verticalMist * 0.10, 0.0, 1.0));
    localFogColor = clamp(localFogColor * fogShade, 0.0, 1.0);

    if (isSkyPixel) {
        color = mix(color, localFogColor, fogFactor * 0.10);
    } else {
        float fogContrastFade = 1.0 - fogFactor * 0.22;
        color = mix(vec3(dot(color, lumaWeights)), color, fogContrastFade);
        color = mix(color, localFogColor, fogFactor * mistAmount);
    }

    if (uEnableBloom != 0) {
        color += bloomGlow(vTexCoord, texelSize) * uBloomIntensity;
    }

    color = max(color * uExposure, 0.0);
    if (uEnableToneMap != 0 && uToneMapMode == 1) {
        color = acesFitted(color);
    }

    color = applySplitTone(color);

    float colorLuma = dot(color, lumaWeights);
    color = mix(vec3(colorLuma), color, uSaturation);
    color = (color - 0.5) * uContrast + 0.5;
    color = clamp(color, 0.0, 1.0);

    if (uEnableVignette != 0) {
        vec2 vignetteUv = vTexCoord * 2.0 - 1.0;
        float vignette = 1.0 - smoothstep(uVignetteSoftness, 1.35, length(vignetteUv));
        color *= mix(1.0, vignette, uVignetteStrength);
    }

    if (uEnableScanlines != 0) {
        float lines = 0.5 + 0.5 * sin(vTexCoord.y * texSize.y * 3.14159265 * uScanlineDensity);
        color *= 1.0 - lines * uScanlineAmount * 0.18;
    }

    if (uEnableGrain != 0) {
        float grain = hash12(vTexCoord * texSize + vec2(uTimeSeconds * 37.1, uTimeSeconds * 53.7)) - 0.5;
        color = clamp(color + grain * uGrainAmount, 0.0, 1.0);
    }

    if (uGamma != 1.0) {
        color = pow(max(color, vec3(0.0)), vec3(1.0 / max(uGamma, 0.001)));
    }

    fragColor = vec4(color, 1.0);
}
