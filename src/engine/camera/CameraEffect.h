#pragma once

#include "engine/camera/CameraState.h"

class CameraEffect {
public:
    virtual ~CameraEffect() = default;
    virtual void update(float deltaTime) = 0;
    virtual CameraState apply(const CameraState& input) const = 0;
    virtual bool isFinished() const = 0;
};
