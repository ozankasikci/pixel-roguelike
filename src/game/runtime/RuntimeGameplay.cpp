#include "game/runtime/RuntimeGameplay.h"

#include "engine/physics/PhysicsSystem.h"
#include "game/components/CameraComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/CheckpointComponent.h"
#include "game/components/ControllableTag.h"
#include "game/components/DoorComponent.h"
#include "game/components/DoorLeafComponent.h"
#include "game/components/InteractableComponent.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerMovementComponent.h"
#include "game/components/PlayerSpawnComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/PrimaryCameraTag.h"
#include "game/components/StaticColliderComponent.h"
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

bool inventoryTogglePressed(const RuntimeInputState& input) {
    return input.isKeyJustPressed(GLFW_KEY_I)
        || input.isKeyJustPressedByName("i")
        || input.isKeyJustPressedByName("I")
        || input.isKeyJustPressedByName("ı")
        || input.isKeyJustPressedByName("İ")
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

float getDoorLeafYaw(const DoorLeafComponent& leaf, float progress) {
    const float eased = 1.0f - std::pow(1.0f - progress, 3.0f);
    return glm::mix(leaf.closedYaw, leaf.openYaw, eased);
}

glm::mat4 makeDoorLeafModel(const DoorLeafComponent& leaf, float progress) {
    const float yaw = getDoorLeafYaw(leaf, progress);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), leaf.hingePosition);
    model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::translate(model, leaf.centerOffsetFromHinge);
    model = glm::scale(model, leaf.closedScale);
    return model;
}

bool isPlayerLocked(entt::registry& registry, entt::entity player) {
    if (const auto* lock = registry.try_get<PlayerInteractionLockComponent>(player)) {
        return lock->active;
    }
    return false;
}

void updateDoorLeaf(entt::registry& registry, entt::entity entity, float progress) {
    auto* mesh = registry.try_get<MeshComponent>(entity);
    auto* collider = registry.try_get<StaticColliderComponent>(entity);
    auto* leaf = registry.try_get<DoorLeafComponent>(entity);
    if (!mesh || !collider || !leaf) {
        return;
    }

    const float yaw = getDoorLeafYaw(*leaf, progress);
    const glm::mat4 model = makeDoorLeafModel(*leaf, progress);
    const glm::vec3 center = glm::vec3(model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

    mesh->modelOverride = model;
    mesh->useModelOverride = true;

    collider->position = center;
    collider->rotation = glm::vec3(0.0f, yaw, 0.0f);
    collider->halfExtents = leaf->colliderHalfExtents;
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

void updateRuntimeInteraction(entt::registry& registry, const RuntimeInputState& input) {
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

    if (focus.focused == entt::null) {
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

    if (input.isCursorLocked() && !input.wantsCaptureMouse() && input.isKeyJustPressed(GLFW_KEY_E)) {
        focus.activationRequested = true;
    }
}

void initializeRuntimeInventory(entt::registry& registry) {
    (void)ensureMenuState(registry);
    (void)ensureInventoryCaptureState(registry);
}

void updateRuntimeInventory(entt::registry& registry,
                            RuntimeInputState& input,
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

void initializeRuntimeDoors(entt::registry& registry) {
    auto lockView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : lockView.each()) {
        (void)entity;
        lock.active = false;
        lock.remainingTime = 0.0f;
    }

    auto doorView = registry.view<DoorComponent>();
    for (auto [entity, door] : doorView.each()) {
        (void)entity;
        if (registry.try_get<DoorLeafComponent>(door.leftLeaf) != nullptr) {
            updateDoorLeaf(registry, door.leftLeaf, 0.0f);
        }
        if (registry.try_get<DoorLeafComponent>(door.rightLeaf) != nullptr) {
            updateDoorLeaf(registry, door.rightLeaf, 0.0f);
        }
    }
}

void updateRuntimeDoors(entt::registry& registry, float deltaTime) {
    auto& ctx = registry.ctx();
    if (!ctx.contains<InteractionFocusState>()) {
        ctx.emplace<InteractionFocusState>();
    }
    auto& focus = ctx.get<InteractionFocusState>();

    entt::entity playerEntity = entt::null;
    PlayerInteractionLockComponent* playerLock = nullptr;

    auto playerView = registry.view<PlayerInteractionLockComponent>();
    for (auto [entity, lock] : playerView.each()) {
        playerEntity = entity;
        playerLock = &lock;
        break;
    }

    if (playerLock && playerLock->active) {
        playerLock->remainingTime = std::max(0.0f, playerLock->remainingTime - deltaTime);
        if (playerLock->remainingTime <= 0.0f) {
            playerLock->active = false;
        }
    }

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

    if (focus.focused == entt::null || !focus.activationRequested || focus.activationConsumed) {
        return;
    }

    auto* door = registry.try_get<DoorComponent>(focus.focused);
    if (door == nullptr) {
        return;
    }

    auto& interactable = registry.get<InteractableComponent>(focus.focused);
    auto& resolvedDoor = *door;
    if (resolvedDoor.opened || resolvedDoor.opening) {
        return;
    }

    if (isPlayerLocked(registry, playerEntity)) {
        return;
    }

    resolvedDoor.opening = true;
    resolvedDoor.progress = 0.0f;
    interactable.busy = true;
    focus.activationConsumed = true;

    if (playerLock) {
        playerLock->active = true;
        playerLock->remainingTime = resolvedDoor.openDuration * 0.9f;
    }
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

    if (focus.focused == entt::null || !focus.activationRequested || focus.activationConsumed) {
        return;
    }

    auto* checkpoint = registry.try_get<CheckpointComponent>(focus.focused);
    if (checkpoint == nullptr || checkpoint->active) {
        return;
    }

    auto checkpointViewAll = registry.view<CheckpointComponent>();
    for (auto [entity, other] : checkpointViewAll.each()) {
        (void)entity;
        other.active = false;
    }

    checkpoint->active = true;
    playerSpawn->respawnPosition = checkpoint->respawnPosition;
    session.respawnPosition = checkpoint->respawnPosition;
    feedback.messageTimer = 2.0f;
    focus.activationConsumed = true;

    if (auto* interactable = registry.try_get<InteractableComponent>(focus.focused)) {
        interactable->busy = true;
    }
}

void updateRuntimePlayerMovement(entt::registry& registry,
                                 const RuntimeInputState& input,
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
                         const RuntimeInputState& input,
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
