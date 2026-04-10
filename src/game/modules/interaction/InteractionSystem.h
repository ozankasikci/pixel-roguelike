#pragma once

#include "engine/ecs/GameRegistry.h"

class CameraManager;
class InputSystem;
struct InteractionPromptState;

void initializeRuntimeInteraction(GameRegistry& registry);
void resetRuntimeInteraction(GameRegistry& registry);
void clearRuntimeInteraction(GameRegistry& registry);
InteractionPromptState& ensureInteractionPromptState(GameRegistry& registry);
void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input,
                              const CameraManager& cameraManager);
