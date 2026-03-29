#include "game/rendering/EnvironmentDebugSync.h"

#include "game/content/ContentRegistry.h"

#include <algorithm>

namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

struct PreservedDebugFields {
    int debugViewMode = 0;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    float timeSeconds = 0.0f;
};

PreservedDebugFields preserveDebugFields(const DebugParams& params) {
    return PreservedDebugFields{
        params.post.debugViewMode,
        params.post.nearPlane,
        params.post.farPlane,
        params.post.timeSeconds,
    };
}

void restoreDebugFields(DebugParams& params, const PreservedDebugFields& preserved) {
    params.post.debugViewMode = preserved.debugViewMode;
    params.post.nearPlane = preserved.nearPlane;
    params.post.farPlane = preserved.farPlane;
    params.post.timeSeconds = preserved.timeSeconds;
}

bool shouldApply(const RuntimeEnvironmentSyncState* syncState,
                 bool usedOverride,
                 const ActiveEnvironmentProfile& active) {
    if (syncState == nullptr || !syncState->hasApplied) {
        return true;
    }
    return syncState->usedOverride != usedOverride
        || syncState->levelId != active.levelId
        || syncState->environmentId != active.environmentId
        || syncState->profile != active.profile;
}

void storeApplied(RuntimeEnvironmentSyncState* syncState,
                  bool usedOverride,
                  const ActiveEnvironmentProfile& active) {
    if (syncState == nullptr) {
        return;
    }
    syncState->hasApplied = true;
    syncState->usedOverride = usedOverride;
    syncState->levelId = active.levelId;
    syncState->environmentId = active.environmentId;
    syncState->profile = active.profile;
}

} // namespace

void syncSkySunFromDirectional(DebugParams& params) {
    params.post.sky.sunDirection = safeNormalize(params.sunDirectional.direction, params.post.sky.sunDirection);
    params.post.sky.sunColor = glm::max(params.sunDirectional.color, glm::vec3(0.0f));
}

void applyEnvironmentSettings(DebugParams& params,
                              const EnvironmentRenderSettings& settings,
                              bool preserveDebugOverrides) {
    const PreservedDebugFields preserved = preserveDebugOverrides
        ? preserveDebugFields(params)
        : PreservedDebugFields{};

    params.post = settings.post;
    params.post.sky = settings.sky;

    if (preserveDebugOverrides) {
        restoreDebugFields(params, preserved);
    }

    params.hemisphereSkyColor = settings.lighting.hemisphereSkyColor;
    params.hemisphereGroundColor = settings.lighting.hemisphereGroundColor;
    params.hemisphereStrength = settings.lighting.hemisphereStrength;
    params.enableDirectionalLights = settings.lighting.enableDirectionalLights;
    params.sunDirectional = settings.lighting.sun;
    params.fillDirectional = settings.lighting.fill;
    syncSkySunFromDirectional(params);
}

void applyEnvironmentDefinition(DebugParams& params,
                                const EnvironmentDefinition& definition,
                                bool preserveDebugOverrides) {
    applyEnvironmentSettings(params, makeEnvironmentRenderSettings(definition), preserveDebugOverrides);
}

void applyEnvironmentProfile(DebugParams& params,
                             EnvironmentProfile profile,
                             bool preserveDebugOverrides) {
    applyEnvironmentSettings(params, makeEnvironmentRenderSettings(profile), preserveDebugOverrides);
}

void syncEnvironmentFromRegistry(entt::registry& registry,
                                 DebugParams& params,
                                 RuntimeEnvironmentSyncState* syncState,
                                 bool preserveDebugOverrides) {
    auto& ctx = registry.ctx();

    if (ctx.contains<RuntimeEnvironmentOverride>()) {
        ActiveEnvironmentProfile active{};
        if (ctx.contains<ActiveEnvironmentProfile>()) {
            active = ctx.get<ActiveEnvironmentProfile>();
        } else {
            active.levelId = "runtime_preview";
            active.environmentId = ctx.get<RuntimeEnvironmentOverride>().definition.id;
            active.profile = EnvironmentProfile::Neutral;
        }
        if (shouldApply(syncState, true, active)) {
            applyEnvironmentDefinition(params, ctx.get<RuntimeEnvironmentOverride>().definition, preserveDebugOverrides);
            storeApplied(syncState, true, active);
        }
        return;
    }

    if (!ctx.contains<ActiveEnvironmentProfile>()) {
        ActiveEnvironmentProfile neutral{};
        neutral.environmentId = "neutral";
        neutral.profile = EnvironmentProfile::Neutral;
        if (shouldApply(syncState, false, neutral)) {
            applyEnvironmentProfile(params, EnvironmentProfile::Neutral, preserveDebugOverrides);
            storeApplied(syncState, false, neutral);
        }
        return;
    }

    const auto& active = ctx.get<ActiveEnvironmentProfile>();
    if (!shouldApply(syncState, false, active)) {
        return;
    }

    if (ctx.contains<ContentRegistry*>()) {
        if (const ContentRegistry* content = ctx.get<ContentRegistry*>(); content != nullptr) {
            if (const auto* environment = content->findEnvironment(active.environmentId)) {
                applyEnvironmentDefinition(params, *environment, preserveDebugOverrides);
                storeApplied(syncState, false, active);
                return;
            }
        }
    }

    applyEnvironmentProfile(params, active.profile, preserveDebugOverrides);
    storeApplied(syncState, false, active);
}
