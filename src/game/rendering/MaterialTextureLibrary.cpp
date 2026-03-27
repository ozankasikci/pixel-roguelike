#include "game/rendering/MaterialTextureLibrary.h"

#include "engine/core/PathUtils.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"

#include <glm/glm.hpp>

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
    materials_.clear();
    textureSets_.clear();
    createFallbackTextures();

    for (const auto& [id, definition] : content.materials()) {
        (void)definition;
        const auto resolved = resolveMaterialDefinition(id, content.materials());

        TextureSet textures;
        bool useMaterialMaps = false;

        if (resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick) {
            buildBrickSet(textures);
            useMaterialMaps = true;
        } else {
            if (!resolved.albedoMapPath.empty()) {
                textures.albedo.createRGBA8FromFile(resolveProjectPath(resolved.albedoMapPath));
                useMaterialMaps = true;
            }
            if (!resolved.normalMapPath.empty()) {
                textures.normal.createRGBA8FromFile(resolveProjectPath(resolved.normalMapPath));
                useMaterialMaps = true;
            }
            if (!resolved.roughnessMapPath.empty()) {
                textures.roughness.createR8FromFile(resolveProjectPath(resolved.roughnessMapPath));
                useMaterialMaps = true;
            }
            if (!resolved.aoMapPath.empty()) {
                textures.ao.createR8FromFile(resolveProjectPath(resolved.aoMapPath));
                useMaterialMaps = true;
            }
        }

        auto [it, inserted] = textureSets_.emplace(id, std::move(textures));
        (void)inserted;
        const TextureSet& storedTextures = it->second;

        RenderMaterialData renderMaterial;
        renderMaterial.id = resolved.id;
        renderMaterial.shadingModel = resolved.shadingModel;
        renderMaterial.baseColor = resolved.baseColor;
        renderMaterial.useMaterialMaps = useMaterialMaps;
        renderMaterial.albedoTexture = storedTextures.albedo.id() != 0 ? storedTextures.albedo.id() : fallbackTextures_.albedo.id();
        renderMaterial.normalTexture = storedTextures.normal.id() != 0 ? storedTextures.normal.id() : fallbackTextures_.normal.id();
        renderMaterial.roughnessTexture = storedTextures.roughness.id() != 0 ? storedTextures.roughness.id() : fallbackTextures_.roughness.id();
        renderMaterial.aoTexture = storedTextures.ao.id() != 0 ? storedTextures.ao.id() : fallbackTextures_.ao.id();
        renderMaterial.uvMode = static_cast<int>(resolved.uvMode);
        renderMaterial.uvScale = resolved.uvScale;
        renderMaterial.normalStrength = resolved.normalStrength;
        renderMaterial.roughnessScale = resolved.roughnessScale;
        renderMaterial.roughnessBias = resolved.roughnessBias;
        renderMaterial.metalness = resolved.metalness;
        renderMaterial.aoStrength = resolved.aoStrength;
        renderMaterial.lightTintResponse = resolved.lightTintResponse;
        materials_.emplace(renderMaterial.id, renderMaterial);
    }
}

const RenderMaterialData& MaterialTextureLibrary::resolve(std::string_view materialId, MaterialKind legacyKind) const {
    if (!materialId.empty()) {
        auto it = materials_.find(std::string(materialId));
        if (it != materials_.end()) {
            return it->second;
        }
    }

    auto fallback = materials_.find(std::string(defaultMaterialIdForKind(legacyKind)));
    if (fallback != materials_.end()) {
        return fallback->second;
    }

    auto stone = materials_.find("stone_default");
    if (stone != materials_.end()) {
        return stone->second;
    }

    throw std::runtime_error("Material library missing stone_default fallback");
}

void MaterialTextureLibrary::buildBrickSet(TextureSet& brick) const {
    constexpr int kSize = 512;
    constexpr int kCourseCount = 12;
    constexpr float kBricksPerRow = 6.0f;

    std::vector<std::uint8_t> albedo(static_cast<size_t>(kSize * kSize * 4), 255);
    std::vector<std::uint8_t> normal(static_cast<size_t>(kSize * kSize * 4), 255);
    std::vector<std::uint8_t> roughness(static_cast<size_t>(kSize * kSize), 255);
    std::vector<std::uint8_t> ao(static_cast<size_t>(kSize * kSize), 255);
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
            albedo[colorIndex + 0] = toByte(color.r);
            albedo[colorIndex + 1] = toByte(color.g);
            albedo[colorIndex + 2] = toByte(color.b);
            albedo[colorIndex + 3] = 255;
            roughness[pixelIndex] = toByte(localRoughness);
            ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
        }
    }

    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            normal[colorIndex + 3] = 255;
        }
    }

    brick.albedo.createRGBA8(kSize, kSize, albedo);
    brick.normal.createRGBA8(kSize, kSize, normal);
    brick.roughness.createR8(kSize, kSize, roughness);
    brick.ao.createR8(kSize, kSize, ao);
}
