#include "engine/core/Window.h"
#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/post/PostProcessParams.h"
#include "engine/rendering/post/StylizePass.h"
#include "engine/ui/Screenshot.h"
#include "game/levels/cathedral/CathedralAssets.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/RetroPalette.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct ViewerPreset {
    std::string name;
    glm::vec3 tint;
    MaterialKind material;
    glm::vec3 modelScale{1.0f};
    glm::vec3 modelRotation{0.0f};
    glm::vec3 target{0.0f};
    float distance = 4.0f;
    float yawDeg = 90.0f;
    float pitchDeg = 10.0f;
};

struct ViewerConfig {
    std::string initialMesh = "door_leaf_left";
    std::string screenshotPath;
    bool listOnly = false;
    int width = 1440;
    int height = 960;
};

std::vector<ViewerPreset> buildPresets() {
    return {
        {"door_leaf_left",  glm::vec3(0.33f, 0.15f, 0.09f), MaterialKind::Wood,  glm::vec3(2.315f, 6.26f, 0.42f), glm::vec3(0.0f),  glm::vec3(0.0f, 0.0f, 0.0f), 5.8f, 90.0f, 2.0f},
        {"door_leaf_right", glm::vec3(0.26f, 0.11f, 0.07f), MaterialKind::Wood,  glm::vec3(2.315f, 6.26f, 0.42f), glm::vec3(0.0f),  glm::vec3(0.0f, 0.0f, 0.0f), 5.8f, 90.0f, 2.0f},
        {"door_frame_romanesque", glm::vec3(0.82f, 0.80f, 0.75f), MaterialKind::Stone, glm::vec3(4.25f, 6.25f, 0.90f), glm::vec3(0.0f), glm::vec3(0.0f, 3.12f, 0.0f), 8.4f, 90.0f, 5.0f},
        {"gothic_door_static", glm::vec3(0.09f, 0.09f, 0.10f), MaterialKind::Wood, glm::vec3(6.45f),           glm::vec3(0.0f, 90.0f, 0.0f), glm::vec3(0.0f, 3.1f, 0.0f), 9.2f, 90.0f, 6.0f},
        {"pillar",          RetroPalette::CarvedStone,      MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 2.8f, 0.0f), 6.2f, 40.0f, 8.0f},
        {"arch",            RetroPalette::CarvedStone,      MaterialKind::Stone, glm::vec3(1.2f, 1.15f, 1.0f),  glm::vec3(0.0f),  glm::vec3(0.0f, 6.0f, 0.0f), 8.2f, 90.0f, 5.0f},
        {"hand",            RetroPalette::Bone,             MaterialKind::Viewmodel, glm::vec3(2.5f),            glm::vec3(-10.0f, 210.0f, 0.0f), glm::vec3(0.0f), 2.6f, 110.0f, 15.0f},
        {"cube",            RetroPalette::Stone,            MaterialKind::Stone, glm::vec3(2.0f),                glm::vec3(0.0f),  glm::vec3(0.0f), 3.6f, 45.0f, 18.0f},
        {"plane",           RetroPalette::Flagstone,        MaterialKind::Floor, glm::vec3(4.0f),                glm::vec3(0.0f),  glm::vec3(0.0f), 4.8f, 60.0f, 60.0f},
        {"cylinder",        RetroPalette::Stone,            MaterialKind::Stone, glm::vec3(1.6f, 3.0f, 1.6f),   glm::vec3(0.0f),  glm::vec3(0.0f, 1.5f, 0.0f), 5.2f, 45.0f, 10.0f},
        {"cylinder_wide",   RetroPalette::Stone,            MaterialKind::Stone, glm::vec3(1.1f, 1.4f, 1.1f),   glm::vec3(0.0f),  glm::vec3(0.0f, 0.7f, 0.0f), 4.2f, 45.0f, 16.0f},
        {"cylinder_cap",    RetroPalette::Stone,            MaterialKind::Stone, glm::vec3(1.1f, 1.0f, 1.1f),   glm::vec3(0.0f),  glm::vec3(0.0f, 0.5f, 0.0f), 4.2f, 45.0f, 16.0f},
    };
}

ViewerConfig parseArgs(int argc, char* argv[]) {
    ViewerConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--mesh" && i + 1 < argc) {
            config.initialMesh = argv[++i];
        } else if (arg == "--screenshot" && i + 1 < argc) {
            config.screenshotPath = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::max(320, std::atoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::max(240, std::atoi(argv[++i]));
        } else if (arg == "--list") {
            config.listOnly = true;
        }
    }
    return config;
}

glm::mat4 makeModel(const ViewerPreset& preset, float spinDeg) {
    glm::mat4 model(1.0f);
    model = glm::rotate(model, glm::radians(preset.modelRotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(preset.modelRotation.y + spinDeg), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(preset.modelRotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, preset.modelScale);
    return model;
}

glm::vec3 orbitOffset(float yawDeg, float pitchDeg, float distance) {
    const float yaw = glm::radians(yawDeg);
    const float pitch = glm::radians(glm::clamp(pitchDeg, -85.0f, 85.0f));
    return glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)
    ) * distance;
}

std::string modeLabel(bool stylized, const PostProcessParams& params, bool autoRotate) {
    return std::string(stylized ? "stylized" : "debug")
        + (params.enableDither ? " +dither" : " -dither")
        + (params.enableEdges ? " +edges" : " -edges")
        + (autoRotate ? " +spin" : "");
}

RenderMaterialData makeViewerMaterial(MaterialKind kind) {
    RenderMaterialData material;
    material.id = std::string(defaultMaterialIdForKind(kind));
    material.shadingModel = kind;
    material.baseColor = glm::vec3(1.0f);
    return material;
}

} // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    const ViewerConfig config = parseArgs(argc, argv);
    std::vector<ViewerPreset> presets = buildPresets();

    if (config.listOnly) {
        for (const auto& preset : presets) {
            spdlog::info("{}", preset.name);
        }
        return 0;
    }

    auto presetIt = std::find_if(presets.begin(), presets.end(), [&](const ViewerPreset& preset) {
        return preset.name == config.initialMesh;
    });
    std::size_t presetIndex = presetIt != presets.end()
        ? static_cast<std::size_t>(std::distance(presets.begin(), presetIt))
        : 0;

    Window window(config.width, config.height, "Procedural Model Viewer");
    glfwSwapInterval(1);

    MeshLibrary meshLibrary;
    registerCathedralAssets(meshLibrary);
    meshLibrary.loadFromFile("gothic_door_static", "assets/meshes/gothic_door_static.glb");

    std::unique_ptr<Shader> sceneShader = std::make_unique<Shader>(
        "assets/shaders/game/scene.vert",
        "assets/shaders/game/scene.frag"
    );
    Renderer renderer(sceneShader.get());
    CompositePass compositePass;
    StylizePass stylizePass;
    Framebuffer sceneFbo;
    Framebuffer compositeFbo;
    sceneFbo.create(window.width(), window.height());
    compositeFbo.create(window.width(), window.height());

    PostProcessParams params;
    params.enableFog = false;
    params.enableDither = false;
    params.enableEdges = false;
    params.enableBloom = false;
    params.enableVignette = false;
    params.enableGrain = false;
    params.enableScanlines = false;
    params.enableSharpen = false;
    params.exposure = 0.58f;
    params.contrast = 1.0f;
    params.saturation = 0.94f;

    AutoScreenshot autoCapture;
    if (!config.screenshotPath.empty()) {
        autoCapture.enable(config.screenshotPath, 12);
    }

    std::unordered_map<int, bool> previousKeys;
    auto isJustPressed = [&](int key) {
        const bool down = glfwGetKey(window.handle(), key) == GLFW_PRESS;
        const bool pressed = down && !previousKeys[key];
        previousKeys[key] = down;
        return pressed;
    };

    auto currentPreset = [&]() -> const ViewerPreset& {
        return presets[presetIndex];
    };

    float orbitYaw = currentPreset().yawDeg;
    float orbitPitch = currentPreset().pitchDeg;
    float orbitDistance = currentPreset().distance;
    float autoSpinDeg = 0.0f;
    bool autoRotate = false;
    bool stylizedPreview = false;
    bool f12Held = false;

    spdlog::info("Procedural Model Viewer controls:");
    spdlog::info("  [ / ]  previous/next mesh");
    spdlog::info("  arrows orbit camera, =/- zoom, R reset camera");
    spdlog::info("  TAB toggles debug/stylized preview, D toggles dither, E toggles edges, SPACE toggles spin");
    spdlog::info("  F12 saves screenshot, ESC quits");

    while (!window.shouldClose()) {
        window.pollEvents();

        if (isJustPressed(GLFW_KEY_ESCAPE)) {
            break;
        }

        if (isJustPressed(GLFW_KEY_LEFT_BRACKET)) {
            presetIndex = (presetIndex + presets.size() - 1) % presets.size();
            orbitYaw = currentPreset().yawDeg;
            orbitPitch = currentPreset().pitchDeg;
            orbitDistance = currentPreset().distance;
        }
        if (isJustPressed(GLFW_KEY_RIGHT_BRACKET)) {
            presetIndex = (presetIndex + 1) % presets.size();
            orbitYaw = currentPreset().yawDeg;
            orbitPitch = currentPreset().pitchDeg;
            orbitDistance = currentPreset().distance;
        }
        if (isJustPressed(GLFW_KEY_R)) {
            orbitYaw = currentPreset().yawDeg;
            orbitPitch = currentPreset().pitchDeg;
            orbitDistance = currentPreset().distance;
            autoSpinDeg = 0.0f;
        }
        if (isJustPressed(GLFW_KEY_TAB)) {
            stylizedPreview = !stylizedPreview;
            params.enableDither = stylizedPreview;
            params.enableEdges = stylizedPreview;
            params.enableBloom = stylizedPreview;
            params.enableVignette = stylizedPreview;
            params.enableGrain = stylizedPreview;
        }
        if (isJustPressed(GLFW_KEY_D)) {
            params.enableDither = !params.enableDither;
        }
        if (isJustPressed(GLFW_KEY_E)) {
            params.enableEdges = !params.enableEdges;
        }
        if (isJustPressed(GLFW_KEY_SPACE)) {
            autoRotate = !autoRotate;
        }

        const float orbitStep = 0.9f;
        const float zoomStep = 0.08f;
        if (glfwGetKey(window.handle(), GLFW_KEY_LEFT) == GLFW_PRESS) orbitYaw -= orbitStep;
        if (glfwGetKey(window.handle(), GLFW_KEY_RIGHT) == GLFW_PRESS) orbitYaw += orbitStep;
        if (glfwGetKey(window.handle(), GLFW_KEY_UP) == GLFW_PRESS) orbitPitch = glm::clamp(orbitPitch + orbitStep, -85.0f, 85.0f);
        if (glfwGetKey(window.handle(), GLFW_KEY_DOWN) == GLFW_PRESS) orbitPitch = glm::clamp(orbitPitch - orbitStep, -85.0f, 85.0f);
        if (glfwGetKey(window.handle(), GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window.handle(), GLFW_KEY_KP_ADD) == GLFW_PRESS) {
            orbitDistance = std::max(1.2f, orbitDistance - zoomStep);
        }
        if (glfwGetKey(window.handle(), GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window.handle(), GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
            orbitDistance = std::min(20.0f, orbitDistance + zoomStep);
        }

        if (autoRotate) {
            autoSpinDeg += 0.35f;
        }

        const int displayW = window.width();
        const int displayH = window.height();
        if (sceneFbo.width() != displayW || sceneFbo.height() != displayH) {
            sceneFbo.resize(displayW, displayH);
            compositeFbo.resize(displayW, displayH);
        }

        const ViewerPreset& preset = currentPreset();
        Mesh* mesh = meshLibrary.get(preset.name);
        if (mesh == nullptr) {
            spdlog::error("Viewer mesh '{}' not found", preset.name);
            return 1;
        }

        const glm::vec3 cameraPos = preset.target + orbitOffset(orbitYaw, orbitPitch, orbitDistance);
        const glm::mat4 view = glm::lookAt(cameraPos, preset.target, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(displayW) / static_cast<float>(displayH), 0.1f, 100.0f);

        const glm::mat4 model = makeModel(preset, autoSpinDeg);
        std::vector<RenderObject> objects = {
            RenderObject{mesh, model, preset.tint, makeViewerMaterial(preset.material)}
        };

        Mesh* plane = meshLibrary.get("plane");
        if (plane != nullptr && preset.name != "plane") {
            glm::mat4 ground(1.0f);
            ground = glm::translate(ground, glm::vec3(0.0f, -3.2f, 0.0f));
            ground = glm::scale(ground, glm::vec3(10.0f, 1.0f, 10.0f));
            objects.push_back(RenderObject{plane, ground, glm::vec3(0.24f, 0.24f, 0.27f), makeViewerMaterial(MaterialKind::Stone)});
        }

        std::vector<RenderLight> lights = {
            {LightType::Point, preset.target + glm::vec3(2.2f, 2.8f, 3.4f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.98f, 0.95f), 14.0f, 0.44f},
            {LightType::Point, preset.target + glm::vec3(-2.5f, 1.8f, 2.2f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.82f, 0.86f, 0.95f), 12.0f, 0.26f},
            {LightType::Point, preset.target + glm::vec3(0.0f, 2.0f, -3.8f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.96f, 0.94f, 1.00f), 10.0f, 0.14f},
            {LightType::Point, cameraPos + glm::vec3(0.0f, 0.8f, 0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f), 8.0f, 0.10f}
        };
        LightingEnvironment lighting;
        lighting.hemisphereSkyColor = glm::vec3(0.18f, 0.19f, 0.22f);
        lighting.hemisphereGroundColor = glm::vec3(0.07f, 0.07f, 0.08f);
        lighting.hemisphereStrength = 0.42f;
        lighting.enableShadows = false;
        ShadowRenderData shadowData;

        sceneFbo.bind();
        glViewport(0, 0, sceneFbo.width(), sceneFbo.height());
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.09f, 0.09f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.drawScene(objects, lights, lighting, shadowData, view, projection, cameraPos);
        sceneFbo.unbind();
        glDisable(GL_DEPTH_TEST);

        params.nearPlane = 0.1f;
        params.farPlane = 100.0f;
        params.timeSeconds = static_cast<float>(glfwGetTime());

        compositePass.apply(sceneFbo.colorTexture(),
                            sceneFbo.depthTexture(),
                            sceneFbo.normalTexture(),
                            compositeFbo.framebuffer(),
                            params,
                            compositeFbo.width(),
                            compositeFbo.height());

        glViewport(0, 0, displayW, displayH);
        glClear(GL_COLOR_BUFFER_BIT);
        stylizePass.apply(compositeFbo.colorTexture(),
                          sceneFbo.colorTexture(),
                          sceneFbo.depthTexture(),
                          sceneFbo.normalTexture(),
                          params,
                          displayW, displayH);

        if (glfwGetKey(window.handle(), GLFW_KEY_F12) == GLFW_PRESS && !f12Held) {
            f12Held = true;
            saveScreenshot(displayW, displayH);
        }
        if (glfwGetKey(window.handle(), GLFW_KEY_F12) == GLFW_RELEASE) {
            f12Held = false;
        }
        if (autoCapture.tick(displayW, displayH)) {
            break;
        }

        glfwSetWindowTitle(
            window.handle(),
            ("Procedural Model Viewer | mesh=" + preset.name
             + " | " + modeLabel(stylizedPreview, params, autoRotate)
             + " | [ ] mesh  arrows orbit  +/- zoom  F12 shot").c_str()
        );

        window.swapBuffers();
    }

    return 0;
}
