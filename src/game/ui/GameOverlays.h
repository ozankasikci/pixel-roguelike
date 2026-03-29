#pragma once

struct PlayerMovementComponent;
struct ViewmodelComponent;
class ContentRegistry;
struct EffectiveEquipmentView;
struct InventoryMenuState;
struct RunSession;

namespace GameOverlays {

void renderMovementOverlay(PlayerMovementComponent& movement, bool grounded);
void renderViewmodelOverlay(ViewmodelComponent& vm);
void renderInteractionPrompt(const char* text, bool busy);
void renderInventory(InventoryMenuState& menu,
                     const RunSession& session,
                     const ContentRegistry& content,
                     const EffectiveEquipmentView& equipment);

} // namespace GameOverlays
