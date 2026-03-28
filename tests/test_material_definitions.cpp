#include "game/rendering/MaterialDefinition.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>

namespace {

bool nearlyEqual(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto base = loadMaterialDefinitionAsset(MATERIAL_BASE_FILE);
    const auto brick = loadMaterialDefinitionAsset(MATERIAL_BRICK_FILE);
    const auto brickOld = loadMaterialDefinitionAsset(MATERIAL_BRICK_OLD_FILE);

    assert(base.id == "masonry_base");
    assert(base.shadingModel.has_value() && *base.shadingModel == MaterialKind::Stone);
    assert(brick.id == "brick_default");
    assert(brick.parent.has_value() && *brick.parent == "masonry_base");
    assert(brickOld.parent.has_value() && *brickOld.parent == "brick_default");

    std::unordered_map<std::string, MaterialDefinition> definitions;
    definitions.emplace(base.id, base);
    definitions.emplace(brick.id, brick);
    definitions.emplace(brickOld.id, brickOld);

    const auto resolved = resolveMaterialDefinition("brick_wall_old", definitions);
    assert(resolved.shadingModel == MaterialKind::Brick);
    assert(resolved.uvMode == MaterialUvMode::WorldProjected);
    assert(nearlyEqual(resolved.uvScale.x, 0.16f));
    assert(nearlyEqual(resolved.uvScale.y, 0.16f));
    assert(nearlyEqual(resolved.baseColor.x, 0.98f));
    assert(nearlyEqual(resolved.baseColor.y, 0.95f));
    assert(nearlyEqual(resolved.baseColor.z, 0.92f));
    assert(resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick);

    {
        std::unordered_map<std::string, MaterialDefinition> missingParent;
        MaterialDefinition child;
        child.id = "child";
        child.parent = "missing";
        missingParent.emplace(child.id, child);
        bool threw = false;
        try {
            (void)resolveMaterialDefinition("child", missingParent);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    {
        std::unordered_map<std::string, MaterialDefinition> cycle;
        MaterialDefinition a;
        a.id = "a";
        a.parent = "b";
        a.shadingModel = MaterialKind::Stone;
        MaterialDefinition b;
        b.id = "b";
        b.parent = "a";
        b.shadingModel = MaterialKind::Brick;
        cycle.emplace(a.id, a);
        cycle.emplace(b.id, b);
        bool threw = false;
        try {
            (void)resolveMaterialDefinition("a", cycle);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    {
        MaterialDefinition roundtrip;
        roundtrip.id = "editor_roundtrip_material";
        roundtrip.parent = "masonry_base";
        roundtrip.shadingModel = MaterialKind::Stone;
        roundtrip.baseColor = glm::vec3(0.8f, 0.7f, 0.6f);
        roundtrip.uvMode = MaterialUvMode::WorldProjected;
        roundtrip.uvScale = glm::vec2(0.2f, 0.25f);
        roundtrip.normalStrength = 0.45f;
        roundtrip.roughnessScale = 1.2f;
        roundtrip.metalness = 0.1f;
        roundtrip.proceduralSource = MaterialProceduralSource::GeneratedStone;

        const auto path = std::filesystem::temp_directory_path() / "editor_roundtrip_material.material";
        saveMaterialDefinitionAsset(path.string(), roundtrip);
        const auto loaded = loadMaterialDefinitionAsset(path.string());
        std::filesystem::remove(path);

        assert(loaded.id == roundtrip.id);
        assert(loaded.parent == roundtrip.parent);
        assert(loaded.shadingModel == roundtrip.shadingModel);
        assert(nearlyEqual(loaded.baseColor->x, roundtrip.baseColor->x));
        assert(nearlyEqual(loaded.baseColor->y, roundtrip.baseColor->y));
        assert(nearlyEqual(loaded.baseColor->z, roundtrip.baseColor->z));
        assert(loaded.uvMode == roundtrip.uvMode);
        assert(nearlyEqual(loaded.uvScale->x, roundtrip.uvScale->x));
        assert(nearlyEqual(loaded.uvScale->y, roundtrip.uvScale->y));
        assert(nearlyEqual(*loaded.normalStrength, *roundtrip.normalStrength));
        assert(nearlyEqual(*loaded.roughnessScale, *roundtrip.roughnessScale));
        assert(nearlyEqual(*loaded.metalness, *roundtrip.metalness));
        assert(loaded.proceduralSource == roundtrip.proceduralSource);
    }

    return 0;
}
