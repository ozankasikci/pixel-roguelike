#include "editor/render/EditorViewportRenderer.h"

#include "engine/rendering/SceneRenderPipeline.h"
#include "game/rendering/EnvironmentDefinition.h"

void EditorViewportRenderer::init() {
    pipeline_.init();
}

void EditorViewportRenderer::shutdown() {
    pipeline_.shutdown();
}

void EditorViewportRenderer::render(const EditorViewportRenderParams& params,
                                     int internalW, int internalH,
                                     int outputW, int outputH,
                                     GLuint targetFBO) {
    if (!params.objects || !params.lights || !params.environment) {
        return;
    }

    SceneRenderInput input;
    input.objects = params.objects;
    input.viewmodelObjects = nullptr;
    input.lights = params.lights;
    input.viewMatrix = params.viewMatrix;
    input.projectionMatrix = params.projectionMatrix;
    input.cameraPosition = params.cameraPosition;
    input.nearPlane = params.nearPlane;
    input.farPlane = params.farPlane;
    input.postParams = &params.environment->post;
    input.shadowsEnabled = params.shadowsEnabled;
    input.shadowResolutionIndex = params.shadowResolutionIndex;

    // Map EnvironmentDefinition lighting fields to LightingEnvironment
    const auto& envLighting = params.environment->lighting;
    input.lightingEnvironment.hemisphereSkyColor = envLighting.hemisphereSkyColor;
    input.lightingEnvironment.hemisphereGroundColor = envLighting.hemisphereGroundColor;
    input.lightingEnvironment.hemisphereStrength = envLighting.hemisphereStrength;
    input.lightingEnvironment.enableDirectionalLights = envLighting.enableDirectionalLights;
    input.lightingEnvironment.sun = envLighting.sun;
    input.lightingEnvironment.fill = envLighting.fill;
    input.lightingEnvironment.enableShadows = envLighting.enableShadows;
    input.lightingEnvironment.shadowBias = envLighting.shadowBias;
    input.lightingEnvironment.shadowNormalBias = envLighting.shadowNormalBias;

    pipeline_.render(input, internalW, internalH, outputW, outputH, targetFBO);
}
