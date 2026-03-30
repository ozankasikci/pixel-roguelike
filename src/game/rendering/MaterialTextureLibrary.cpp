#include "game/rendering/MaterialTextureLibrary.h"

#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/AssetCache.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

float saturate(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float smooth01(float edge0, float edge1, float x) {
    float t = saturate((x - edge0) / std::max(edge1 - edge0, 1e-6f));
    return t * t * (3.0f - 2.0f * t);
}

float hash21(const glm::vec2& p) {
    return glm::fract(std::sin(glm::dot(p, glm::vec2(127.1f, 311.7f))) * 43758.5453f);
}

float valueNoise(const glm::vec2& p) {
    glm::vec2 i = glm::floor(p);
    glm::vec2 f = glm::fract(p);
    f = f * f * (glm::vec2(3.0f) - 2.0f * f);

    float n00 = hash21(i + glm::vec2(0.0f, 0.0f));
    float n10 = hash21(i + glm::vec2(1.0f, 0.0f));
    float n01 = hash21(i + glm::vec2(0.0f, 1.0f));
    float n11 = hash21(i + glm::vec2(1.0f, 1.0f));

    float nx0 = glm::mix(n00, n10, f.x);
    float nx1 = glm::mix(n01, n11, f.x);
    return glm::mix(nx0, nx1, f.y);
}

float fbm(glm::vec2 p) {
    float value = 0.0f;
    float amplitude = 0.5f;
    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        p = p * 2.03f + glm::vec2(19.1f, 7.7f);
        amplitude *= 0.5f;
    }
    return value;
}

// Tileable noise: lattice coordinates wrap at the given period so the pattern
// repeats seamlessly.  A vec2 seed shifts the hash input without breaking tiling.
float tileableValueNoise(const glm::vec2& p, float period, const glm::vec2& seed) {
    glm::vec2 ip = glm::floor(p);
    glm::vec2 fp = p - ip;
    fp = fp * fp * (glm::vec2(3.0f) - 2.0f * fp);

    auto wrap = [period](glm::vec2 v) {
        return glm::vec2(v.x - period * std::floor(v.x / period),
                         v.y - period * std::floor(v.y / period));
    };

    float n00 = hash21(wrap(ip) + seed);
    float n10 = hash21(wrap(ip + glm::vec2(1.0f, 0.0f)) + seed);
    float n01 = hash21(wrap(ip + glm::vec2(0.0f, 1.0f)) + seed);
    float n11 = hash21(wrap(ip + glm::vec2(1.0f, 1.0f)) + seed);

    return glm::mix(glm::mix(n00, n10, fp.x), glm::mix(n01, n11, fp.x), fp.y);
}

float tileableFbm(glm::vec2 p, float baseFreq, const glm::vec2& seed) {
    float value = 0.0f;
    float amplitude = 0.5f;
    float freq = baseFreq;
    for (int i = 0; i < 4; ++i) {
        value += tileableValueNoise(p * freq, freq, seed + glm::vec2(float(i) * 5.3f, float(i) * 3.7f)) * amplitude;
        freq *= 2.0f;
        amplitude *= 0.5f;
    }
    return value;
}

std::uint8_t toByte(float value) {
    return static_cast<std::uint8_t>(std::round(saturate(value) * 255.0f));
}

glm::vec3 sampleHeightNormal(const std::vector<float>& heights, int size, int x, int y) {
    auto wrap = [size](int value) {
        value %= size;
        return value < 0 ? value + size : value;
    };

    float hL = heights[wrap(y) * size + wrap(x - 1)];
    float hR = heights[wrap(y) * size + wrap(x + 1)];
    float hD = heights[wrap(y - 1) * size + wrap(x)];
    float hU = heights[wrap(y + 1) * size + wrap(x)];

    glm::vec3 n((hL - hR) * 3.2f, (hD - hU) * 3.2f, 1.0f);
    return glm::normalize(n);
}

} // namespace

void MaterialTextureLibrary::createFallbackTextures() {
    fallbackTextures_.albedo.createRGBA8(1, 1, {255, 255, 255, 255});
    fallbackTextures_.normal.createRGBA8(1, 1, {128, 128, 255, 255});
    fallbackTextures_.roughness.createR8(1, 1, {204});
    fallbackTextures_.ao.createR8(1, 1, {255});
}

void MaterialTextureLibrary::init(const ContentRegistry& content) {
    resolvedDefinitions_.clear();
    materials_.clear();
    textureSets_.clear();
    createFallbackTextures();

    for (const auto& [id, definition] : content.materials()) {
        (void)definition;
        resolvedDefinitions_.emplace(id, resolveMaterialDefinition(id, content.materials()));
    }
}

std::size_t MaterialTextureLibrary::prewarmAllMaterialMaps() const {
    std::size_t warmedTextureSets = 0;
    for (const auto& [id, resolved] : resolvedDefinitions_) {
        (void)id;
        const bool useMaterialMaps =
            resolved.proceduralSource != MaterialProceduralSource::None ||
            !resolved.albedoMapPath.empty() ||
            !resolved.normalMapPath.empty() ||
            !resolved.roughnessMapPath.empty() ||
            !resolved.aoMapPath.empty();
        if (!useMaterialMaps) {
            continue;
        }

        const std::string key = textureKeyFor(resolved);
        if (textureSets_.find(key) == textureSets_.end()) {
            (void)ensureTextureSet(resolved);
            ++warmedTextureSets;
        }
    }
    return warmedTextureSets;
}

const RenderMaterialData& MaterialTextureLibrary::resolve(std::string_view materialId, MaterialKind legacyKind) const {
    const ResolvedMaterialDefinition& resolved = definitionFor(materialId, legacyKind);
    auto cached = materials_.find(resolved.id);
    if (cached != materials_.end()) {
        return cached->second;
    }

    const bool useMaterialMaps =
        resolved.proceduralSource != MaterialProceduralSource::None ||
        !resolved.albedoMapPath.empty() ||
        !resolved.normalMapPath.empty() ||
        !resolved.roughnessMapPath.empty() ||
        !resolved.aoMapPath.empty();

    const TextureSet* textures = useMaterialMaps ? &ensureTextureSet(resolved) : nullptr;

    RenderMaterialData renderMaterial;
    renderMaterial.id = resolved.id;
    renderMaterial.shadingModel = resolved.shadingModel;
    renderMaterial.baseColor = resolved.baseColor;
    renderMaterial.useMaterialMaps = useMaterialMaps;
    renderMaterial.useProceduralDetail =
        resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick ||
        resolved.proceduralSource == MaterialProceduralSource::GeneratedStone;
    renderMaterial.albedoTexture = (textures != nullptr && textures->albedo.id() != 0) ? textures->albedo.id() : fallbackTextures_.albedo.id();
    renderMaterial.normalTexture = (textures != nullptr && textures->normal.id() != 0) ? textures->normal.id() : fallbackTextures_.normal.id();
    renderMaterial.roughnessTexture = (textures != nullptr && textures->roughness.id() != 0) ? textures->roughness.id() : fallbackTextures_.roughness.id();
    renderMaterial.aoTexture = (textures != nullptr && textures->ao.id() != 0) ? textures->ao.id() : fallbackTextures_.ao.id();
    renderMaterial.uvMode = static_cast<int>(resolved.uvMode);
    renderMaterial.uvScale = resolved.uvScale;
    renderMaterial.normalStrength = resolved.normalStrength;
    renderMaterial.roughnessScale = resolved.roughnessScale;
    renderMaterial.roughnessBias = resolved.roughnessBias;
    renderMaterial.metalness = resolved.metalness;
    renderMaterial.aoStrength = resolved.aoStrength;
    renderMaterial.lightTintResponse = resolved.lightTintResponse;
    renderMaterial.emissiveStrength = resolved.emissiveStrength;

    auto [it, inserted] = materials_.emplace(renderMaterial.id, renderMaterial);
    (void)inserted;
    return it->second;
}

const ResolvedMaterialDefinition& MaterialTextureLibrary::definitionFor(std::string_view materialId,
                                                                        MaterialKind legacyKind) const {
    if (!materialId.empty()) {
        auto it = resolvedDefinitions_.find(std::string(materialId));
        if (it != resolvedDefinitions_.end()) {
            return it->second;
        }
    }

    auto fallback = resolvedDefinitions_.find(std::string(defaultMaterialIdForKind(legacyKind)));
    if (fallback != resolvedDefinitions_.end()) {
        return fallback->second;
    }

    auto stone = resolvedDefinitions_.find("stone_default");
    if (stone != resolvedDefinitions_.end()) {
        return stone->second;
    }

    throw std::runtime_error("Material library missing stone_default fallback");
}

const MaterialTextureLibrary::TextureSet& MaterialTextureLibrary::ensureTextureSet(const ResolvedMaterialDefinition& resolved) const {
    const std::string key = textureKeyFor(resolved);
    auto cached = textureSets_.find(key);
    if (cached != textureSets_.end()) {
        return cached->second;
    }

    TextureSet textures;

    const bool isProcedural =
        resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick ||
        resolved.proceduralSource == MaterialProceduralSource::GeneratedStone ||
        resolved.proceduralSource == MaterialProceduralSource::GeneratedSmooth ||
        resolved.proceduralSource == MaterialProceduralSource::GeneratedFloor;

    if (isProcedural) {
        // Try disk cache for procedural textures
        uint64_t paramHash = AssetCache::hashBytes(key.data(), key.size());
        auto cachedAlbedo = AssetCache::findTextureCache(key + "_albedo", paramHash);
        auto cachedNormal = AssetCache::findTextureCache(key + "_normal", paramHash);
        auto cachedRoughness = AssetCache::findTextureCache(key + "_roughness", paramHash);
        auto cachedAo = AssetCache::findTextureCache(key + "_ao", paramHash);

        if (cachedAlbedo && cachedNormal && cachedRoughness && cachedAo) {
            textures.albedo.createRGBA8(cachedAlbedo->width, cachedAlbedo->height, cachedAlbedo->pixels);
            textures.normal.createRGBA8(cachedNormal->width, cachedNormal->height, cachedNormal->pixels);
            textures.roughness.createR8(cachedRoughness->width, cachedRoughness->height, cachedRoughness->pixels);
            textures.ao.createR8(cachedAo->width, cachedAo->height, cachedAo->pixels);
            auto [it, inserted] = textureSets_.emplace(key, std::move(textures));
            (void)inserted;
            return it->second;
        }

        // Cache miss: generate pixels, create GL textures, write to disk cache
        ProceduralPixelData pixels;
        if (resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick) {
            pixels = generateBrickPixels();
        } else if (resolved.proceduralSource == MaterialProceduralSource::GeneratedSmooth) {
            pixels = generateSmoothWallPixels();
        } else if (resolved.proceduralSource == MaterialProceduralSource::GeneratedFloor) {
            pixels = generateFloorPixels();
        } else {
            pixels = generateStonePixels();
        }

        textures.albedo.createRGBA8(pixels.size, pixels.size, pixels.albedo);
        textures.normal.createRGBA8(pixels.size, pixels.size, pixels.normal);
        textures.roughness.createR8(pixels.size, pixels.size, pixels.roughness);
        textures.ao.createR8(pixels.size, pixels.size, pixels.ao);

        // Write to disk cache for next launch
        uint16_t sz = static_cast<uint16_t>(pixels.size);
        AssetCache::writeTextureCache(key + "_albedo", paramHash, pixels.albedo, sz, sz, 4);
        AssetCache::writeTextureCache(key + "_normal", paramHash, pixels.normal, sz, sz, 4);
        AssetCache::writeTextureCache(key + "_roughness", paramHash, pixels.roughness, sz, sz, 1);
        AssetCache::writeTextureCache(key + "_ao", paramHash, pixels.ao, sz, sz, 1);
    } else {
        // File-based textures: not cached per design doc (diminishing returns)
        if (!resolved.albedoMapPath.empty()) {
            textures.albedo.createRGBA8FromFile(resolveProjectPath(resolved.albedoMapPath));
        }
        if (!resolved.normalMapPath.empty()) {
            textures.normal.createRGBA8FromFile(resolveProjectPath(resolved.normalMapPath));
        }
        if (!resolved.roughnessMapPath.empty()) {
            textures.roughness.createR8FromFile(resolveProjectPath(resolved.roughnessMapPath));
        }
        if (!resolved.aoMapPath.empty()) {
            textures.ao.createR8FromFile(resolveProjectPath(resolved.aoMapPath));
        }
    }

    auto [it, inserted] = textureSets_.emplace(key, std::move(textures));
    (void)inserted;
    return it->second;
}

std::string MaterialTextureLibrary::textureKeyFor(const ResolvedMaterialDefinition& resolved) const {
    return std::to_string(static_cast<int>(resolved.proceduralSource))
        + "|" + resolved.albedoMapPath
        + "|" + resolved.normalMapPath
        + "|" + resolved.roughnessMapPath
        + "|" + resolved.aoMapPath;
}

MaterialTextureLibrary::ProceduralPixelData MaterialTextureLibrary::generateBrickPixels() const {
    constexpr int kSize = 512;
    constexpr int kCourseCount = 12;
    constexpr float kBricksPerRow = 6.0f;

    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);

            const float courseV = v * static_cast<float>(kCourseCount);
            const int course = std::min(kCourseCount - 1, static_cast<int>(std::floor(courseV)));
            const float localV = glm::fract(courseV);
            const float courseU = u * kBricksPerRow + std::fmod(static_cast<float>(course), 2.0f) * 0.5f;
            const float brickIndex = std::floor(courseU);
            const glm::vec2 brickCell(brickIndex, static_cast<float>(course));
            const glm::vec2 brickLocal(glm::fract(courseU), localV);
            const glm::vec2 brickUv(courseU, localV + static_cast<float>(course));

            const float seamDistX = std::min(brickLocal.x, 1.0f - brickLocal.x);
            const float seamDistY = std::min(brickLocal.y, 1.0f - brickLocal.y);
            const float seamDist = std::min(seamDistX, seamDistY);
            const float mortar = 1.0f - smooth01(0.040f, 0.075f, seamDist);

            const glm::vec2 centered = glm::abs(brickLocal - glm::vec2(0.5f)) * 2.0f;
            const float face = 1.0f - smooth01(0.12f, 0.90f, std::max(centered.x, centered.y));
            const float brickSeed = hash21(brickCell);
            const float brickVariant = hash21(brickCell + glm::vec2(3.4f, 8.1f));
            const float brickHueShift = hash21(brickCell + glm::vec2(11.7f, 2.3f)) - 0.5f;
            const float macro = fbm(brickCell * 0.47f + glm::vec2(1.2f, 6.4f));
            const float pores = fbm(brickUv * glm::vec2(3.2f, 3.8f) + glm::vec2(2.8f, 7.1f));
            const float faceNoise = fbm(brickUv * glm::vec2(1.2f, 1.9f) + brickCell * 0.41f);
            const float streaks = fbm(brickUv * glm::vec2(1.8f, 4.8f) + brickCell * 0.58f);
            const float chips = smooth01(0.68f, 0.92f, fbm(brickUv * glm::vec2(1.8f, 2.2f) + brickCell * 0.73f));
            const float soot = smooth01(0.62f, 0.88f, fbm(brickCell * 0.35f + glm::vec2(9.4f, 1.7f)));
            const float paleChance = 1.0f - smooth01(0.10f, 0.28f, brickVariant);
            const float darkChance = smooth01(0.82f, 0.97f, brickVariant);
            const float repairChance = smooth01(0.89f, 0.98f, hash21(brickCell + glm::vec2(17.3f, 6.2f)));

            const glm::vec3 redBrick(0.82f, 0.64f, 0.58f);
            const glm::vec3 warmBrick(0.92f, 0.72f, 0.64f);
            const glm::vec3 paleBrick(1.00f, 0.80f, 0.74f);
            const glm::vec3 darkBrick(0.68f, 0.53f, 0.48f);
            glm::vec3 mortarColor = glm::mix(glm::vec3(0.72f, 0.68f, 0.64f),
                                             glm::vec3(0.84f, 0.80f, 0.76f),
                                             hash21(glm::vec2(course, brickCell.x + 5.1f)) * 0.35f + macro * 0.10f);

            glm::vec3 brickColor = glm::mix(redBrick, warmBrick, brickSeed * 0.42f + macro * 0.12f);
            brickColor = glm::mix(brickColor, paleBrick, paleChance * 0.30f);
            brickColor = glm::mix(brickColor, darkBrick, darkChance * (0.28f + soot * 0.16f));
            brickColor *= glm::vec3(1.0f + brickHueShift * 0.05f,
                                    1.0f - std::abs(brickHueShift) * 0.02f,
                                    1.0f - brickHueShift * 0.03f);
            brickColor *= 0.93f + pores * 0.04f + face * 0.03f + faceNoise * 0.04f;
            brickColor = glm::mix(brickColor, brickColor * glm::vec3(1.05f, 1.03f, 1.00f), smooth01(0.66f, 0.90f, streaks) * 0.08f);
            brickColor = glm::mix(brickColor, brickColor * glm::vec3(0.88f, 0.85f, 0.83f), chips * 0.05f + soot * 0.06f);
            brickColor = glm::mix(brickColor, mortarColor * glm::vec3(1.02f, 1.01f, 0.99f), repairChance * 0.08f);
            glm::vec3 color = glm::mix(brickColor, mortarColor, mortar * 0.94f);

            float localHeight = face * (0.015f + brickSeed * 0.003f)
                - mortar * 0.028f
                - chips * 0.006f
                + (pores - 0.5f) * 0.003f;
            float localAo = 1.0f - mortar * 0.14f - chips * 0.03f - darkChance * 0.03f;
            float localRoughness = glm::mix(0.80f + pores * 0.05f, 0.92f, mortar * 0.76f) + darkChance * 0.02f - paleChance * 0.02f;

            const size_t pixelIndex = static_cast<size_t>(y * kSize + x);
            const size_t colorIndex = pixelIndex * 4;
            result.albedo[colorIndex + 0] = toByte(color.r);
            result.albedo[colorIndex + 1] = toByte(color.g);
            result.albedo[colorIndex + 2] = toByte(color.b);
            result.albedo[colorIndex + 3] = 255;
            result.roughness[pixelIndex] = toByte(localRoughness);
            result.ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
        }
    }

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            result.normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            result.normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            result.normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            result.normal[colorIndex + 3] = 255;
        }
    }

    return result;
}

void MaterialTextureLibrary::buildBrickSet(TextureSet& brick) const {
    auto pixels = generateBrickPixels();
    brick.albedo.createRGBA8(pixels.size, pixels.size, pixels.albedo);
    brick.normal.createRGBA8(pixels.size, pixels.size, pixels.normal);
    brick.roughness.createR8(pixels.size, pixels.size, pixels.roughness);
    brick.ao.createR8(pixels.size, pixels.size, pixels.ao);
}

MaterialTextureLibrary::ProceduralPixelData MaterialTextureLibrary::generateStonePixels() const {
    constexpr int kSize = 512;

    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);
            glm::vec2 p(u, v);

            const float macro = fbm(p * 1.2f + glm::vec2(2.1f, 5.4f));
            const float mid = fbm(p * 3.4f + glm::vec2(7.2f, 1.3f));
            const float micro = fbm(p * 13.5f + glm::vec2(4.7f, 11.6f));
            const float grain = fbm(p * 22.0f + glm::vec2(13.4f, 8.9f));
            const float veins = smooth01(0.58f, 0.88f, 1.0f - std::abs(std::sin((u * 7.0f + v * 5.6f + macro * 1.8f) * 3.14159f)));
            const float damp = smooth01(0.60f, 0.84f, fbm(p * glm::vec2(2.0f, 4.6f) + glm::vec2(9.1f, 3.7f)));
            const float specks = smooth01(0.76f, 0.94f, fbm(p * 30.0f + glm::vec2(6.6f, 17.2f)));

            glm::vec3 baseA(0.77f, 0.75f, 0.71f);
            glm::vec3 baseB(0.69f, 0.68f, 0.63f);
            glm::vec3 warmPatch(0.83f, 0.80f, 0.74f);
            glm::vec3 coolPatch(0.63f, 0.65f, 0.64f);

            glm::vec3 color = glm::mix(baseA, baseB, macro * 0.70f);
            color = glm::mix(color, warmPatch, mid * 0.16f);
            color = glm::mix(color, coolPatch, damp * 0.10f);
            color *= 0.95f + micro * 0.10f + grain * 0.04f;
            color = glm::mix(color, color * glm::vec3(1.04f, 1.03f, 1.00f), veins * 0.10f);
            color = glm::mix(color, color * glm::vec3(0.84f, 0.86f, 0.84f), damp * 0.08f);
            color = glm::mix(color, color * glm::vec3(0.78f, 0.76f, 0.74f), specks * 0.04f);

            float localHeight = (micro - 0.5f) * 0.010f
                + (grain - 0.5f) * 0.004f
                - veins * 0.004f
                - damp * 0.003f
                - specks * 0.002f;
            float localRoughness = 0.72f + damp * 0.10f + (1.0f - veins) * 0.06f + grain * 0.04f;
            float localAo = 0.96f - damp * 0.08f - veins * 0.04f - specks * 0.03f;

            const size_t pixelIndex = static_cast<size_t>(y * kSize + x);
            const size_t colorIndex = pixelIndex * 4;
            result.albedo[colorIndex + 0] = toByte(color.r);
            result.albedo[colorIndex + 1] = toByte(color.g);
            result.albedo[colorIndex + 2] = toByte(color.b);
            result.albedo[colorIndex + 3] = 255;
            result.roughness[pixelIndex] = toByte(localRoughness);
            result.ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
        }
    }

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            result.normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            result.normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            result.normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            result.normal[colorIndex + 3] = 255;
        }
    }

    return result;
}

MaterialTextureLibrary::ProceduralPixelData MaterialTextureLibrary::generateSmoothWallPixels() const {
    constexpr int kSize = 512;

    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);
            glm::vec2 p(u, v);

            // All noise is tileable over [0,1] UV range
            const float broad = tileableFbm(p, 2.0f, glm::vec2(3.1f, 7.4f));
            const float mid   = tileableFbm(p, 4.0f, glm::vec2(11.3f, 2.8f));
            const float fine  = tileableFbm(p, 16.0f, glm::vec2(5.9f, 14.2f));
            const float patch = tileableFbm(p, 2.0f, glm::vec2(8.7f, 4.1f));

            // Warm, muted base — Stanley Parable aesthetic
            glm::vec3 baseWarm(0.88f, 0.85f, 0.80f);
            glm::vec3 baseCool(0.82f, 0.81f, 0.78f);
            glm::vec3 color = glm::mix(baseWarm, baseCool, broad * 0.5f + patch * 0.2f);
            color *= 0.96f + mid * 0.06f + fine * 0.02f;

            // Very subtle height for normal map — smooth surface with micro-grain
            float localHeight = (mid - 0.5f) * 0.004f + (fine - 0.5f) * 0.002f;
            float localRoughness = 0.68f + mid * 0.06f + fine * 0.04f;
            float localAo = 0.98f - patch * 0.03f;

            const size_t pixelIndex = static_cast<size_t>(y * kSize + x);
            const size_t colorIndex = pixelIndex * 4;
            result.albedo[colorIndex + 0] = toByte(color.r);
            result.albedo[colorIndex + 1] = toByte(color.g);
            result.albedo[colorIndex + 2] = toByte(color.b);
            result.albedo[colorIndex + 3] = 255;
            result.roughness[pixelIndex] = toByte(localRoughness);
            result.ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
        }
    }

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            result.normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            result.normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            result.normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            result.normal[colorIndex + 3] = 255;
        }
    }

    return result;
}

MaterialTextureLibrary::ProceduralPixelData MaterialTextureLibrary::generateFloorPixels() const {
    constexpr int kSize = 512;
    constexpr float kTileCount = 4.0f; // Large-format institutional floor tiles

    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);
            glm::vec2 p(u, v);

            // Tile grid — faint seams between large-format floor tiles
            // Offset by half a tile so seams land at tile centers, not texture edges
            const float tileU = u * kTileCount + 0.5f;
            const float tileV = v * kTileCount + 0.5f;
            const float seamDistX = std::min(glm::fract(tileU), 1.0f - glm::fract(tileU));
            const float seamDistY = std::min(glm::fract(tileV), 1.0f - glm::fract(tileV));
            const float seamDist = std::min(seamDistX, seamDistY);
            const float seam = 1.0f - smooth01(0.010f, 0.030f, seamDist); // Very thin, subtle seam

            // Per-tile color variation — wrap cell index so edges tile seamlessly
            const glm::vec2 tileCell(std::fmod(std::floor(tileU), kTileCount),
                                     std::fmod(std::floor(tileV), kTileCount));
            const float tileVariation = hash21(tileCell) * 0.03f - 0.015f;

            // Tileable noise layers for surface micro-variation
            const float broad = tileableFbm(p, 2.0f, glm::vec2(9.3f, 3.7f));
            const float mid = tileableFbm(p, 6.0f, glm::vec2(2.1f, 15.4f));
            const float fine = tileableFbm(p, 14.0f, glm::vec2(13.8f, 6.2f));

            // Warm beige-tan base — Stanley Parable institutional linoleum
            glm::vec3 baseWarm(0.82f, 0.76f, 0.67f);
            glm::vec3 baseCool(0.78f, 0.74f, 0.68f);
            glm::vec3 color = glm::mix(baseWarm, baseCool, broad * 0.4f + 0.3f);
            color += glm::vec3(tileVariation, tileVariation * 0.8f, tileVariation * 0.5f);
            color *= 0.97f + mid * 0.04f + fine * 0.015f;

            // Seam darkening — subtle joint lines
            color = glm::mix(color, color * 0.88f, seam * 0.6f);

            // Slightly glossier than walls — polished linoleum surface
            float localRoughness = 0.45f + mid * 0.10f + fine * 0.06f;
            localRoughness = glm::mix(localRoughness, localRoughness + 0.15f, seam); // Seams rougher

            // Subtle height — nearly flat with faint seam indentation
            float localHeight = (mid - 0.5f) * 0.003f + (fine - 0.5f) * 0.001f;
            localHeight -= seam * 0.008f; // Seams dip slightly

            // AO — faint darkening at seam joints
            float localAo = 0.98f - seam * 0.08f - broad * 0.02f;

            const size_t pixelIndex = static_cast<size_t>(y * kSize + x);
            const size_t colorIndex = pixelIndex * 4;
            result.albedo[colorIndex + 0] = toByte(color.r);
            result.albedo[colorIndex + 1] = toByte(color.g);
            result.albedo[colorIndex + 2] = toByte(color.b);
            result.albedo[colorIndex + 3] = 255;
            result.roughness[pixelIndex] = toByte(localRoughness);
            result.ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
        }
    }

    // Pass 2: Normal map from height field
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            result.normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            result.normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            result.normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            result.normal[colorIndex + 3] = 255;
        }
    }

    return result;
}

void MaterialTextureLibrary::buildStoneSet(TextureSet& stone) const {
    auto pixels = generateStonePixels();
    stone.albedo.createRGBA8(pixels.size, pixels.size, pixels.albedo);
    stone.normal.createRGBA8(pixels.size, pixels.size, pixels.normal);
    stone.roughness.createR8(pixels.size, pixels.size, pixels.roughness);
    stone.ao.createR8(pixels.size, pixels.size, pixels.ao);
}
