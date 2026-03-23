#pragma once

struct CharacterControllerComponent {
    float eyeHeight = 1.6f;          // eye position above capsule bottom
    float capsuleRadius = 0.2f;      // capsule cap radius
    float capsuleHalfHeight = 0.7f;  // capsule cylinder half-height (total height = 2*(halfHeight+radius) = 1.8m)

    // Derived: offset from capsule center to eye position
    float eyeOffset() const {
        return eyeHeight - capsuleHalfHeight - capsuleRadius;
    }
};
