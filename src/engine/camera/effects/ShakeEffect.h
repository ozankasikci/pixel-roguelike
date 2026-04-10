#pragma once

#include "engine/camera/CameraEffect.h"

class ShakeEffect : public CameraEffect {
public:
    float maxTranslationOffset = 0.05f;
    float maxYawOffset = 1.5f;
    float maxPitchOffset = 1.0f;
    float noiseFrequency = 8.0f;
    float traumaDecayRate = 1.5f;

    void addTrauma(float amount);
    float getTrauma() const;

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    float trauma_ = 0.0f;
    float noiseTime_ = 0.0f;
};
