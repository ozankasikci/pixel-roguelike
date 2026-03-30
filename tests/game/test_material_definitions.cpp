#include "game/rendering/MaterialDefinition.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

int main() {
    const auto base = loadMaterialDefinitionAsset(MATERIAL_BASE_FILE);
    const auto brick = loadMaterialDefinitionAsset(MATERIAL_BRICK_FILE);
    const auto brickOld = loadMaterialDefinitionAsset(MATERIAL_BRICK_OLD_FILE);

    assert(base.id == "masonry_base");
    assert(brick.id == "brick_default");
    assert(brick.parent.has_value() && *brick.parent == "masonry_base");
    assert(brickOld.parent.has_value() && *brickOld.parent == "brick_default");

    std::unordered_map<std::string, MaterialDefinition> definitions;
    definitions.emplace(base.id, base);
    definitions.emplace(brick.id, brick);
    definitions.emplace(brickOld.id, brickOld);

    const auto resolved = resolveMaterialDefinition("brick_wall_old", definitions);
    assert(resolved.detailBrick == true);
    assert(resolved.uvMode == MaterialUvMode::WorldProjected);
    assert(test_support::nearlyEqual(resolved.uvScale.x, 0.16f));
    assert(test_support::nearlyEqual(resolved.uvScale.y, 0.16f));
    assert(test_support::nearlyEqual(resolved.baseColor.x, 0.98f));
    assert(test_support::nearlyEqual(resolved.baseColor.y, 0.95f));
    assert(test_support::nearlyEqual(resolved.baseColor.z, 0.92f));
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
        MaterialDefinition b;
        b.id = "b";
        b.parent = "a";
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
        roundtrip.baseColor = glm::vec3(0.8f, 0.7f, 0.6f);
        roundtrip.uvMode = MaterialUvMode::WorldProjected;
        roundtrip.uvScale = glm::vec2(0.2f, 0.25f);
        roundtrip.normalStrength = 0.45f;
        roundtrip.roughnessScale = 1.2f;
        roundtrip.metalness = 0.1f;
        roundtrip.proceduralSource = MaterialProceduralSource::GeneratedStone;
        roundtrip.specularLevel = 0.35f;
        roundtrip.animated = true;
        roundtrip.detailBrick = false;

        const auto path = test_support::tempPath("editor_roundtrip_material.material");
        saveMaterialDefinitionAsset(path.string(), roundtrip);
        const auto loaded = loadMaterialDefinitionAsset(path.string());
        std::filesystem::remove(path);

        assert(loaded.id == roundtrip.id);
        assert(loaded.parent == roundtrip.parent);
        assert(test_support::nearlyEqual(loaded.baseColor->x, roundtrip.baseColor->x));
        assert(test_support::nearlyEqual(loaded.baseColor->y, roundtrip.baseColor->y));
        assert(test_support::nearlyEqual(loaded.baseColor->z, roundtrip.baseColor->z));
        assert(loaded.uvMode == roundtrip.uvMode);
        assert(test_support::nearlyEqual(loaded.uvScale->x, roundtrip.uvScale->x));
        assert(test_support::nearlyEqual(loaded.uvScale->y, roundtrip.uvScale->y));
        assert(test_support::nearlyEqual(*loaded.normalStrength, *roundtrip.normalStrength));
        assert(test_support::nearlyEqual(*loaded.roughnessScale, *roundtrip.roughnessScale));
        assert(test_support::nearlyEqual(*loaded.metalness, *roundtrip.metalness));
        assert(loaded.proceduralSource == roundtrip.proceduralSource);
        assert(loaded.specularLevel.has_value() && test_support::nearlyEqual(*loaded.specularLevel, 0.35f));
        assert(loaded.animated.has_value() && *loaded.animated == true);
        assert(loaded.detailBrick.has_value() && *loaded.detailBrick == false);
    }

    // Test: feature flag parsing
    {
        const auto path = test_support::tempPath("feature_flags_test.material");
        {
            std::ofstream f(path);
            f << "id feature_test\n";
            f << "shading_model stone\n";
            f << "animated true\n";
            f << "detail_brick true\n";
            f << "detail_stone false\n";
            f << "subsurface true\n";
            f << "specular_level 0.46\n";
        }
        const auto def = loadMaterialDefinitionAsset(path.string());
        std::filesystem::remove(path);
        assert(def.animated.has_value() && *def.animated == true);
        assert(def.detailBrick.has_value() && *def.detailBrick == true);
        assert(def.detailStone.has_value() && *def.detailStone == false);
        assert(def.subsurface.has_value() && *def.subsurface == true);
        assert(def.specularLevel.has_value() && test_support::nearlyEqual(*def.specularLevel, 0.46f));
    }

    // Test: feature flag inheritance — parent's detail_stone propagates to child
    {
        std::unordered_map<std::string, MaterialDefinition> defs;
        MaterialDefinition parent;
        parent.id = "parent_stone";
        parent.detailStone = true;
        parent.specularLevel = 0.20f;
        defs.emplace(parent.id, parent);

        MaterialDefinition child;
        child.id = "child_stone";
        child.parent = "parent_stone";
        defs.emplace(child.id, child);

        const auto resolved = resolveMaterialDefinition("child_stone", defs);
        assert(resolved.detailStone == true);
        assert(test_support::nearlyEqual(resolved.specularLevel, 0.20f));
    }

    // Test: feature flag inheritance override — child can override parent
    {
        std::unordered_map<std::string, MaterialDefinition> defs;
        MaterialDefinition parent;
        parent.id = "parent_brick_base";
        parent.detailStone = true;
        defs.emplace(parent.id, parent);

        MaterialDefinition child;
        child.id = "child_brick";
        child.parent = "parent_brick_base";
        child.detailBrick = true;
        child.detailStone = false;
        defs.emplace(child.id, child);

        const auto resolved = resolveMaterialDefinition("child_brick", defs);
        assert(resolved.detailBrick == true);
        assert(resolved.detailStone == false);
    }

    return 0;
}
