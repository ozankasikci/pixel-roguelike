#pragma once

#include <cstdint>

enum class DoorTargetState : uint8_t { Closed, Open };

struct DoorStateComponent {
    float progress = 0.0f;
    DoorTargetState targetState = DoorTargetState::Closed;
};

// Free-function helpers (POD rule: no methods on components)
inline bool isDoorFullyClosed(const DoorStateComponent& s) {
    return s.targetState == DoorTargetState::Closed && s.progress <= 0.0f;
}
inline bool isDoorFullyOpen(const DoorStateComponent& s) {
    return s.targetState == DoorTargetState::Open && s.progress >= 1.0f;
}
inline bool isDoorMoving(const DoorStateComponent& s) {
    return (s.targetState == DoorTargetState::Open && s.progress < 1.0f) ||
           (s.targetState == DoorTargetState::Closed && s.progress > 0.0f);
}
