#include "game/runtime/RuntimeGameplay.h"

#include <spdlog/spdlog.h>
#include "engine/input/InputSystem.h"
#include "game/components/ColliderComponent.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/MeshComponent.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/RuntimeCameraMath.h"
#include "game/session/EquipmentState.h"
#include "game/session/RunSession.h"
#include "game/ui/InteractionFocusState.h"
#include "game/ui/InteractionPromptState.h"
#include "game/ui/InventoryMenuState.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {

glm::vec3 moveTowardXZ(const glm::vec3& current, const glm::vec3& target, float maxDelta) {
    glm::vec2 cur(current.x, current.z);
    glm::vec2 tgt(target.x, target.z);
    glm::vec2 diff = tgt - cur;
    const float dist = glm::length(diff);

    if (dist <= maxDelta || dist < 0.0001f) {
        return glm::vec3(tgt.x, current.y, tgt.y);
    }

    const glm::vec2 result = cur + (diff / dist) * maxDelta;
    return glm::vec3(result.x, current.y, result.y);
}

glm::vec3 cameraForwardFromAngles(float yawDeg, float pitchDeg) {
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    forward.y = std::sin(glm::radians(pitchDeg));
    forward.z = std::sin(glm::radians(yawDeg)) * std::cos(glm::radians(pitchDeg));
    return glm::normalize(forward);
}

bool inventoryTogglePressed(const InputSystem& input) {
    return input.isKeyJustPressed(GLFW_KEY_I)
        || input.isKeyJustPressedByName("i")
        || input.isKeyJustPressedByName("I")
        || input.isKeyJustPressedByName("\xc4\xb1")
        || input.isKeyJustPressedByName("\xc4\xb0")
        || input.wasCharacterTyped('i')
        || input.wasCharacterTyped('I')
        || input.wasCharacterTyped(0x0130)
        || input.wasCharacterTyped(0x0131);
}

InventoryMenuState& ensureMenuState(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InventoryMenuState>()) {
        ctx.emplace<InventoryMenuState>();
    }
    return ctx.get<InventoryMenuState>();
}

RuntimeInventoryCaptureState& ensureInventoryCaptureState(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeInventoryCaptureState>()) {
        ctx.emplace<RuntimeInventoryCaptureState>();
    }
    return ctx.get<RuntimeInventoryCaptureState>();
}

RuntimeCheckpointFeedbackState& ensureCheckpointFeedbackState(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<RuntimeCheckpointFeedbackState>()) {
        ctx.emplace<RuntimeCheckpointFeedbackState>();
    }
    return ctx.get<RuntimeCheckpointFeedbackState>();
}

bool hasPlayerEntity(entt::registry& registry) {
    auto view = registry.view<PlayerTag>();
    for (auto entity : view) {
        (void)entity;
        return true;
    }
    return false;
}

void clampSelection(RunSession& session, InventoryMenuState& menu) {
    if (session.ownedWeapons.empty()) {
        menu.selectedItem = 0;
        return;
    }
    if (menu.selectedItem < 0) {
        menu.selectedItem = 0;
    }
    const int lastIndex = static_cast<int>(session.ownedWeapons.size()) - 1;
    if (menu.selectedItem > lastIndex) {
        menu.selectedItem = lastIndex;
    }
}

} // namespace

void initializeRuntimeInteraction(entt::registry& registry) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionPromptState>()) {
        ctx.emplace<InteractionPromptState>();
    }
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
}

void updateRuntimeInteraction(entt::registry& registry, const InputSystem& input) {
    auto& ctx = registry.ctx();
    initializeRuntimeInteraction(registry);

    auto& prompt = ctx.get<InteractionPromptState>();
    auto& focus = ctx.get<InteractionFocusState>();
    prompt.visible = false;
    prompt.busy = false;
    prompt.text.clear();
    focus = InteractionFocusState{};

    const bool inventoryOpen = ctx.contains<InventoryMenuState>() && ctx.get<InventoryMenuState>().open;
    if (inventoryOpen) {
        return;
    }

    entt::entity actor = entt::null;
    TransformComponent* actorTransform = nullptr;
    CameraComponent* actorCamera = nullptr;
    PlayerInteractionLockComponent* actorLock = nullptr;

    auto actorView = registry.view<TransformComponent, CameraComponent, ControllableTag, PrimaryCameraTag>();
    for (auto entity : actorView) {
        actor = entity;
        actorTransform = &actorView.get<TransformComponent>(entity);
        actorCamera = &actorView.get<CameraComponent>(entity);
        actorLock = registry.try_get<PlayerInteractionLockComponent>(entity);
        break;
    }

    if (actor == entt::null || actorTransform == nullptr || actorCamera == nullptr) {
        return;
    }

    focus.actor = actor;
    const glm::vec3 actorForward = cameraForwardFromAngles(actorCamera->yaw, actorCamera->pitch);

    float bestScore = -1.0f;
    auto interactableView = registry.view<TransformComponent, InteractableComponent>();
    for (auto [entity, transform, interactable] : interactableView.each()) {
        if (!interactable.enabled) {
            continue;
        }

        const glm::vec3 toTarget = transform.position - actorTransform->position;
        const float distance = glm::length(toTarget);
        if (distance > interactable.interactDistance || distance < 0.001f) {
            continue;
        }

        const glm::vec3 direction = toTarget / distance;
        const float facing = glm::dot(actorForward, direction);
        if (facing < interactable.interactDotThreshold) {
            continue;
        }

        const float score = facing - distance * 0.08f;
        if (score > bestScore) {
            bestScore = score;
            focus.focused = entity;
        }
    }

    // Debug: log every frame E is pressed regardless of focus
    if (input.isKeyPressed(GLFW_KEY_E)) {
        spdlog::info("[INTERACT] E pressed | focused={} | cursorLocked={} | wantsMouse={} | justPressed={}",
                     focus.focused != entt::null, input.isCursorLocked(), input.wantsCaptureMouse(),
                     input.isKeyJustPressed(GLFW_KEY_E));
    }

    if (focus.focused == entt::null) {
        // Debug: if E pressed but no focus, log why
        if (input.isKeyPressed(GLFW_KEY_E)) {
            int interactableCount = 0;
            float closestDist = 999.0f;
            for (auto [entity, transform, interactable] : interactableView.each()) {
                interactableCount++;
                float d = glm::length(transform.position - actorTransform->position);
                if (d < closestDist) closestDist = d;
            }
            spdlog::info("[INTERACT] No focus target! interactables={} closestDist={:.2f} playerPos=({:.2f},{:.2f},{:.2f})",
                         interactableCount, closestDist,
                         actorTransform->position.x, actorTransform->position.y, actorTransform->position.z);
        }
        return;
    }

    auto& interactable = registry.get<InteractableComponent>(focus.focused);
    prompt.visible = true;
    prompt.busy = interactable.busy;
    prompt.text = interactable.busy ? interactable.busyText : interactable.promptText;

    const bool locked = actorLock != nullptr && actorLock->active;
    if (locked || interactable.busy) {
        return;
    }

    if (!input.wantsCaptureMouse() && input.isKeyJustPressed(GLFW_KEY_E)) {
        spdlog::info("[INTERACT] ACTIVATING DOOR!");
        focus.activationRequested = true;
    }
}

void initializeRuntimeInventory(entt::registry& registry) {
    (void)ensureMenuState(registry);
    (void)ensureInventoryCaptureState(registry);
}

void updateRuntimeInventory(entt::registry& registry,
                            InputSystem& input,
                            RunSession& session,
                            const ContentRegistry& content) {
    auto& menu = ensureMenuState(registry);
    auto& captureState = ensureInventoryCaptureState(registry);

    clampSelection(session, menu);

    if (inventoryTogglePressed(input)) {
        menu.open = !menu.open;
        if (menu.open) {
            input.setCursorLocked(false);
            captureState.openedByMenu = true;
        } else if (captureState.openedByMenu) {
            input.setCursorLocked(true);
            captureState.openedByMenu = false;
        }
    } else if (menu.open && input.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        menu.open = false;
        if (captureState.openedByMenu) {
            input.setCursorLocked(true);
            captureState.openedByMenu = false;
        }
    }

    if (menu.selectedCategory == 0) {
        menu.targetedHand = EquipmentHand::Right;
    } else if (menu.selectedCategory == 1) {
        menu.targetedHand = EquipmentHand::Left;
    }

    if (!menu.open || !hasPlayerEntity(registry)) {
        return;
    }

    switch (menu.pendingAction) {
    case InventoryMenuState::PendingActionType::Equip:
        if (!menu.pendingWeaponId.empty()) {
            equipWeapon(session, content, menu.pendingHand, menu.pendingWeaponId);
        }
        break;
    case InventoryMenuState::PendingActionType::Unequip:
        unequipWeapon(session, content, menu.pendingHand);
        break;
    case InventoryMenuState::PendingActionType::None:
        break;
    }

    menu.pendingAction = InventoryMenuState::PendingActionType::None;
    menu.pendingWeaponId.clear();
}

void initializeRuntimeCheckpoints(entt::registry& registry) {
    (void)ensureCheckpointFeedbackState(registry);
}

void updateRuntimeCheckpoints(entt::registry& registry, float deltaTime, RunSession& session) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
    auto& focus = ctx.get<InteractionFocusState>();
    auto& feedback = ensureCheckpointFeedbackState(registry);

    if (feedback.messageTimer > 0.0f) {
        feedback.messageTimer = std::max(0.0f, feedback.messageTimer - deltaTime);
    }

    PlayerSpawnComponent* playerSpawn = nullptr;
    auto playerView = registry.view<PlayerSpawnComponent>();
    for (auto [entity, spawn] : playerView.each()) {
        (void)entity;
        playerSpawn = &spawn;
        break;
    }

    if (!playerSpawn) {
        return;
    }

    auto checkpointView = registry.view<TransformComponent, CheckpointComponent>();
    for (auto [entity, transform, checkpoint] : checkpointView.each()) {
        (void)transform;
        if (auto* light = registry.try_get<LightComponent>(checkpoint.lightEntity)) {
            light->intensity = checkpoint.active ? 2.2f : 1.15f;
            light->radius = checkpoint.active ? 10.0f : 7.0f;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->promptText = checkpoint.active ? "RESPAWN ATTUNED" : "E  KINDLE CHECKPOINT";
            interactable->busyText = "CHECKPOINT KINDLED";
            interactable->busy = checkpoint.active && feedback.messageTimer > 0.0f;
        }
    }

}

void updateRuntimePlayerMovement(entt::registry& registry,
                                 const InputSystem& input,
                                 PhysicsSystem& physics,
                                 float deltaTime) {
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    auto view = registry.view<TransformComponent, CameraComponent, PlayerMovementComponent, CharacterControllerComponent,
                              PlayerInteractionLockComponent, PlayerSpawnComponent, PlayerTag, ControllableTag, PrimaryCameraTag>();
    for (auto [entity, transform, cam, movement, cc, lock, spawn] : view.each()) {
        const bool cursorLocked = input.isCursorLocked();
        const bool locked = lock.active;
        const bool gameplayInputEnabled = cursorLocked && !locked && !inventoryOpen;

        if (transform.position.y < spawn.fallRespawnY) {
            physics.setCharacterVelocity(entity, glm::vec3(0.0f));
            physics.setCharacterPosition(entity, spawn.respawnPosition - glm::vec3(0.0f, cc.eyeOffset(), 0.0f));
            movement.velocity = glm::vec3(0.0f);
            movement.jumpHeld = false;
            transform.position = spawn.respawnPosition;
            continue;
        }

        glm::vec3 inputDir(0.0f);
        if (gameplayInputEnabled) {
            const float yawRad = glm::radians(cam.yaw);
            glm::vec3 forward(std::cos(yawRad), 0.0f, std::sin(yawRad));
            forward = glm::normalize(forward);
            const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

            if (input.isKeyPressed(GLFW_KEY_W)) inputDir += forward;
            if (input.isKeyPressed(GLFW_KEY_S)) inputDir -= forward;
            if (input.isKeyPressed(GLFW_KEY_D)) inputDir += right;
            if (input.isKeyPressed(GLFW_KEY_A)) inputDir -= right;

            if (glm::length(inputDir) > 0.001f) {
                inputDir = glm::normalize(inputDir);
            }
        }

        const glm::vec3 desiredVelocity = inputDir * movement.maxGroundSpeed;
        const bool hasInput = glm::length(inputDir) > 0.001f;
        const GroundState groundState = physics.getCharacterGroundState(entity);
        movement.grounded = (groundState == GroundState::OnGround);

        float accel = movement.airAcceleration;
        if (movement.grounded) {
            accel = hasInput ? movement.acceleration : movement.deceleration;
        }

        movement.velocity = moveTowardXZ(movement.velocity, desiredVelocity, accel * deltaTime);

        if (gameplayInputEnabled && movement.grounded && input.isKeyJustPressed(GLFW_KEY_SPACE)) {
            movement.velocity.y = movement.jumpImpulse;
            movement.jumpHeld = true;
            movement.jumpHoldTimer = 0.0f;
        }

        float effectiveGravity = movement.gravity;
        if (movement.jumpHeld && gameplayInputEnabled && input.isKeyPressed(GLFW_KEY_SPACE)
            && movement.velocity.y > 0.0f
            && movement.jumpHoldTimer < movement.maxJumpHoldTime) {
            effectiveGravity = movement.gravity * movement.jumpHoldGravityScale;
            movement.jumpHoldTimer += deltaTime;
        } else {
            movement.jumpHeld = false;
        }

        if (locked || inventoryOpen) {
            movement.jumpHeld = false;
            movement.velocity.x = 0.0f;
            movement.velocity.z = 0.0f;
        }

        movement.velocity.y += effectiveGravity * deltaTime;
        if (movement.grounded && movement.velocity.y < 0.0f) {
            movement.velocity.y = 0.0f;
        }

        physics.setCharacterVelocity(entity, movement.velocity);
        physics.updateCharacter(entity, deltaTime, glm::vec3(0.0f, movement.gravity, 0.0f));

        const glm::vec3 charPos = physics.getCharacterPosition(entity);
        transform.position = charPos + glm::vec3(0.0f, cc.eyeOffset(), 0.0f);
        movement.grounded = (physics.getCharacterGroundState(entity) == GroundState::OnGround);
        break;
    }
}

void updateRuntimeCamera(entt::registry& registry,
                         const InputSystem& input,
                         float aspect,
                         float deltaTime) {
    (void)deltaTime;
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    auto view = registry.view<TransformComponent, CameraComponent, PlayerInteractionLockComponent, ControllableTag, PrimaryCameraTag>();
    for (auto [entity, transform, cam, lock] : view.each()) {
        (void)entity;
        if (input.isCursorLocked() && !lock.active && !inventoryOpen) {
            constexpr float sensitivity = 0.1f;
            const glm::vec2 delta = input.mouseDelta();
            cam.yaw += delta.x * sensitivity;
            cam.pitch -= delta.y * sensitivity;
        }

        cam.pitch = std::clamp(cam.pitch, -89.0f, 89.0f);
        updateRuntimeCameraComponent(transform, cam, aspect);
        break;
    }
}

namespace {

void updateDoorLeaf(entt::registry& registry, entt::entity leafEntity, float progress) {
    if (leafEntity == entt::null) return;
    auto* mesh = registry.try_get<MeshComponent>(leafEntity);
    auto* collider = registry.try_get<ColliderComponent>(leafEntity);
    auto* leaf = registry.try_get<DoorLeafComponent>(leafEntity);
    if (!mesh || !collider || !leaf) return;

    const float yaw = glm::mix(leaf->closedYaw, leaf->openYaw, progress);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), leaf->hingePosition);
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, leaf->centerOffsetFromHinge);
    model = glm::scale(model, leaf->closedScale);
    const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    mesh->modelOverride = model;
    mesh->useModelOverride = true;
    collider->position = center;
    collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
    collider->halfExtents = leaf->colliderHalfExtents;
}

} // namespace

void updateRuntimeDoors(entt::registry& registry, float deltaTime) {
    // Check if player activated a door
    if (registry.ctx().contains<InteractionFocusState>()) {
        auto& focus = registry.ctx().get<InteractionFocusState>();
        if (focus.activationRequested && !focus.activationConsumed && focus.focused != entt::null) {
            auto* door = registry.try_get<DoorComponent>(focus.focused);
            if (door != nullptr && !door->opening && !door->opened) {
                door->opening = true;
                focus.activationConsumed = true;
            }
        }
    }

    // Animate doors
    auto doorView = registry.view<TransformComponent, DoorComponent>();
    for (auto [entity, transform, door] : doorView.each()) {
        (void)transform;
        if (door.opening || door.opened) {
            door.progress = std::min(1.0f, door.progress + deltaTime / door.openDuration);
            updateDoorLeaf(registry, door.leftLeaf, door.progress);
            updateDoorLeaf(registry, door.rightLeaf, door.progress);

            if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                interactable->busy = door.opening;
                interactable->enabled = !door.opened;
            }

            if (door.progress >= 1.0f) {
                door.progress = 1.0f;
                door.opening = false;
                door.opened = true;
                if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
                    interactable->busy = false;
                    interactable->enabled = false;
                }
            }
            continue;
        }
        if (auto* interactable = registry.try_get<InteractableComponent>(entity)) {
            interactable->busy = false;
            interactable->enabled = !door.opened;
        }
    }
}
