#include "engine/input/InputSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/InteractableComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/modules/interaction/InteractionFocusState.h"
#include "game/modules/interaction/InteractionPromptState.h"
#include "game/modules/interaction/InteractionSystem.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cassert>

int main() {
    GameRegistry registry;
    initializeRuntimeInteraction(registry);
    assert(registry.ctx().contains<InteractionPromptState>());
    assert(registry.ctx().contains<InteractionFocusState>());

    const entt::entity player = registry.create();
    CameraComponent camera;
    camera.yaw = 0.0f;
    camera.pitch = 0.0f;
    registry.emplace<TransformComponent>(player, TransformComponent{glm::vec3(0.0f, 1.6f, 0.0f)});
    registry.emplace<CameraComponent>(player, camera);
    registry.emplace<ControllableTag>(player);
    registry.emplace<PrimaryCameraTag>(player);
    registry.emplace<PlayerInteractionLockComponent>(player);

    const entt::entity frontTarget = registry.create();
    registry.emplace<TransformComponent>(frontTarget, TransformComponent{glm::vec3(2.0f, 1.6f, 0.0f)});
    registry.emplace<InteractableComponent>(frontTarget, InteractableComponent{
        .promptText = "OPEN LEVER",
        .busyText = "LEVER LOCKED",
        .interactDistance = 3.0f,
        .interactDotThreshold = 0.4f,
        .enabled = true,
        .busy = false,
    });

    const entt::entity sideTarget = registry.create();
    registry.emplace<TransformComponent>(sideTarget, TransformComponent{glm::vec3(1.0f, 1.6f, 1.0f)});
    registry.emplace<InteractableComponent>(sideTarget, InteractableComponent{
        .promptText = "SIDE TARGET",
        .busyText = "SIDE BUSY",
        .interactDistance = 3.0f,
        .interactDotThreshold = 0.4f,
        .enabled = true,
        .busy = false,
    });

    InputSystem input;
    input.reset();
    input.beginFrame();
    input.setCursorLocked(true);
    input.setKeyPressed(GLFW_KEY_E, true);

    updateRuntimeInteraction(registry, input);

    const auto& prompt = registry.ctx().get<InteractionPromptState>();
    const auto& focus = registry.ctx().get<InteractionFocusState>();
    assert(prompt.visible);
    assert(!prompt.busy);
    assert(prompt.text == "OPEN LEVER");
    assert(focus.actor == player);
    assert(focus.focused == frontTarget);
    assert(focus.activationRequested);
    assert(!focus.activationConsumed);

    registry.ctx().insert_or_assign<InventoryMenuState>(InventoryMenuState{.open = true});
    updateRuntimeInteraction(registry, input);
    assert(!registry.ctx().get<InteractionPromptState>().visible);
    assert(registry.ctx().get<InteractionFocusState>().focused == entt::null);

    registry.ctx().get<InventoryMenuState>().open = false;
    registry.get<PlayerInteractionLockComponent>(player).active = true;
    updateRuntimeInteraction(registry, input);
    assert(registry.ctx().get<InteractionPromptState>().visible);
    assert(!registry.ctx().get<InteractionFocusState>().activationRequested);

    resetRuntimeInteraction(registry);
    assert(!registry.ctx().get<InteractionPromptState>().visible);
    assert(registry.ctx().get<InteractionFocusState>().focused == entt::null);

    clearRuntimeInteraction(registry);
    assert(!registry.ctx().contains<InteractionPromptState>());
    assert(!registry.ctx().contains<InteractionFocusState>());

    return 0;
}
